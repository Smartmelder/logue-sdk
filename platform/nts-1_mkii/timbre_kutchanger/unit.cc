/*
    TIMBRE-KUTCHANGER - Ultimate Timbre Morphing ModFX
    
    TRANSFORM ANY OSCILLATOR:
    - Electric (0-19%) - Synth leads
    - Metallic (20-39%) - Techno/psy
    - Flute (40-59%) - Melodic, acoustic
    - Alt/Mezzo (60-79%) - Warm vocal
    - Soprano (80-100%) - Bright, brilliant
    
    Uses formant filtering, harmonic shaping, ensemble processing
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_modfx.h"
#include "fx_api.h"
#include "utils/float_math.h"

// ========== NaN/Inf CHECK MACRO ==========
#define is_finite(x) ((x) != (x) ? false : ((x) <= 1e10f && (x) >= -1e10f))

// ========== MEMORY ==========

#define DELAY_BUFFER_SIZE 480  // 10ms @ 48kHz

static float *s_delay_buffer_l = nullptr;
static float *s_delay_buffer_r = nullptr;
static uint32_t s_write_pos = 0;

// ========== FORMANT FILTERS ==========

struct FormantFilter {
    float z1, z2;
    float freq;
    float q;
};

static FormantFilter s_formant_l[3];
static FormantFilter s_formant_r[3];

// ========== LFO STATE ==========

static float s_vibrato_phase = 0.f;
static float s_ensemble_phase_l = 0.f;
static float s_ensemble_phase_r = 0.f;

// ========== PARAMETERS ==========

static float s_character = 0.5f;      // 0-1 (0-100%)
static float s_brightness = 0.5f;
static float s_formant = 0.4f;
static float s_motion = 0.3f;
static float s_ensemble = 0.5f;
static float s_harmonic = 0.5f;
static float s_attack = 0.5f;
static float s_mix = 0.75f;
static float s_color = 0.5f;
static float s_depth = 0.6f;

// ========== CHARACTER DETECTION ==========

enum CharacterType {
    CHAR_ELECTRIC,
    CHAR_METALLIC,
    CHAR_FLUTE,
    CHAR_VOCAL_LOW,
    CHAR_VOCAL_HIGH
};

inline CharacterType get_character_type() {
    if (s_character < 0.2f) return CHAR_ELECTRIC;
    if (s_character < 0.4f) return CHAR_METALLIC;
    if (s_character < 0.6f) return CHAR_FLUTE;
    if (s_character < 0.8f) return CHAR_VOCAL_LOW;
    return CHAR_VOCAL_HIGH;
}

// ========== FORMANT FILTER ==========

inline void formant_init(FormantFilter *f, float freq, float q) {
    f->z1 = 0.f;
    f->z2 = 0.f;
    f->freq = freq;
    f->q = q;
}

inline float formant_process(FormantFilter *f, float input) {
    // Simple resonant filter
    float w = 2.f * 3.14159265f * f->freq / 48000.f;
    float cos_w = fx_cosf(w);
    float alpha = fx_sinf(w) / (2.f * f->q);
    
    // Limit Q to prevent self-oscillation
    alpha = clipminmaxf(0.1f, alpha, 2.f);
    
    float b0 = alpha;
    float b1 = 0.f;
    float b2 = -alpha;
    float a0 = 1.f + alpha;
    float a1 = -2.f * cos_w;
    float a2 = 1.f - alpha;
    
    float output = (b0 * input + b1 * f->z1 + b2 * f->z2 - a1 * f->z1 - a2 * f->z2) / a0;
    
    f->z2 = f->z1;
    f->z1 = output;
    
    // Denormal kill
    if (si_fabsf(f->z1) < 1e-15f) f->z1 = 0.f;
    if (si_fabsf(f->z2) < 1e-15f) f->z2 = 0.f;
    
    // Clip filter states
    f->z1 = clipminmaxf(-2.f, f->z1, 2.f);
    f->z2 = clipminmaxf(-2.f, f->z2, 2.f);
    
    if (!is_finite(output)) output = 0.f;
    
    return output;
}

// ========== CHARACTER PROCESSING ==========

inline float process_electric(float input, FormantFilter *formants) {
    // Electric: bright, slightly resonant
    float freq1 = 800.f + s_color * 1200.f;
    float freq2 = 1600.f + s_color * 1400.f;
    float freq3 = 2800.f + s_brightness * 2200.f;
    
    formant_init(&formants[0], freq1, 3.f + s_depth * 2.f);
    formant_init(&formants[1], freq2, 4.f + s_depth * 3.f);
    formant_init(&formants[2], freq3, 2.f + s_brightness * 2.f);
    
    float out = input;
    out = formant_process(&formants[0], out) * 0.4f;
    out += formant_process(&formants[1], input) * 0.3f;
    out += formant_process(&formants[2], input) * 0.3f;
    
    return out + input * 0.3f;
}

inline float process_metallic(float input, FormantFilter *formants) {
    // Metallic: high resonances, comb-like
    float freq1 = 1200.f + s_color * 1800.f;
    float freq2 = 2400.f + s_color * 2600.f;
    float freq3 = 4800.f + s_brightness * 3200.f;
    
    formant_init(&formants[0], freq1, 6.f + s_depth * 4.f);
    formant_init(&formants[1], freq2, 8.f + s_depth * 6.f);
    formant_init(&formants[2], freq3, 4.f + s_brightness * 4.f);
    
    float out = formant_process(&formants[0], input) * 0.35f;
    out += formant_process(&formants[1], input) * 0.35f;
    out += formant_process(&formants[2], input) * 0.3f;
    
    return out + input * 0.2f;
}

inline float process_flute(float input, FormantFilter *formants) {
    // Flute: soft, breathy, rounded
    float freq1 = 400.f + s_color * 600.f;
    float freq2 = 1000.f + s_color * 1000.f;
    float freq3 = 2000.f + s_brightness * 1000.f;
    
    formant_init(&formants[0], freq1, 2.f + s_depth);
    formant_init(&formants[1], freq2, 2.5f + s_depth);
    formant_init(&formants[2], freq3, 1.5f + s_brightness);
    
    float out = formant_process(&formants[0], input) * 0.4f;
    out += formant_process(&formants[1], input) * 0.3f;
    out += formant_process(&formants[2], input) * 0.3f;
    
    // Add breathiness
    float breath = input * (1.f - s_color * 0.3f);
    
    return out + breath * 0.4f;
}

inline float process_vocal_low(float input, FormantFilter *formants) {
    // Alt/Mezzo: warm, body, vocal
    float f = s_formant;
    
    float freq1 = 400.f + f * 300.f;   // Low formant
    float freq2 = 800.f + f * 600.f;   // Mid formant
    float freq3 = 2200.f + f * 800.f;  // High formant
    
    formant_init(&formants[0], freq1, 4.f + s_depth * 2.f);
    formant_init(&formants[1], freq2, 5.f + s_depth * 3.f);
    formant_init(&formants[2], freq3, 3.f + s_brightness * 2.f);
    
    float out = formant_process(&formants[0], input) * 0.35f;
    out += formant_process(&formants[1], input) * 0.35f;
    out += formant_process(&formants[2], input) * 0.3f;
    
    return out + input * 0.25f;
}

inline float process_vocal_high(float input, FormantFilter *formants) {
    // Soprano/Coloratura: bright, brilliant
    float f = s_formant;
    
    float freq1 = 600.f + f * 400.f;
    float freq2 = 1400.f + f * 1000.f;
    float freq3 = 3200.f + f * 1800.f;
    
    formant_init(&formants[0], freq1, 4.f + s_depth * 2.f);
    formant_init(&formants[1], freq2, 6.f + s_depth * 4.f);
    formant_init(&formants[2], freq3, 5.f + s_brightness * 3.f);
    
    float out = formant_process(&formants[0], input) * 0.3f;
    out += formant_process(&formants[1], input) * 0.35f;
    out += formant_process(&formants[2], input) * 0.35f;
    
    return out + input * 0.2f;
}

// ========== ENSEMBLE/CHORUS ==========

inline float process_ensemble(float input, float *phase, float offset) {
    if (s_ensemble < 0.01f) return input;
    
    // Chorus-like detune
    float rate = 0.3f + offset;
    *phase += rate / 48000.f;
    if (*phase >= 1.f) *phase -= 1.f;
    
    float lfo = fx_sinf(*phase * 2.f * 3.14159265f);
    float detune = lfo * s_ensemble * 0.005f;
    
    // Simple pitch shift approximation
    return input * (1.f + detune);
}

// ========== VIBRATO ==========

inline float get_vibrato() {
    if (s_motion < 0.01f) return 0.f;
    
    float rate = 4.f + s_motion * 4.f;
    s_vibrato_phase += rate / 48000.f;
    if (s_vibrato_phase >= 1.f) s_vibrato_phase -= 1.f;
    
    return fx_sinf(s_vibrato_phase * 2.f * 3.14159265f) * s_motion * 0.003f;
}

// ========== HARMONIC EMPHASIS ==========

inline float apply_harmonic_emphasis(float input) {
    // Simple tilt EQ based on harmonic parameter
    static float hp_z1_l = 0.f;
    static float lp_z1_l = 0.f;
    
    // Highpass for high harmonic emphasis
    float hp_coeff = 0.1f + s_harmonic * 0.4f;
    hp_z1_l += hp_coeff * (input - hp_z1_l);
    float hp = input - hp_z1_l;
    
    if (si_fabsf(hp_z1_l) < 1e-15f) hp_z1_l = 0.f;
    
    // Lowpass for low harmonic emphasis
    float lp_coeff = 0.5f - s_harmonic * 0.3f;
    lp_z1_l += lp_coeff * (input - lp_z1_l);
    
    if (si_fabsf(lp_z1_l) < 1e-15f) lp_z1_l = 0.f;
    
    // Blend
    float low_amt = 1.f - s_harmonic;
    float high_amt = s_harmonic;
    
    return lp_z1_l * low_amt + (input + hp * high_amt) * high_amt;
}

// ========== ATTACK SHAPING ==========

inline float apply_attack(float input) {
    // Simple envelope follower for attack shaping
    static float env_l = 0.f;
    
    float abs_input = si_fabsf(input);
    
    float attack_speed = 0.001f + s_attack * 0.01f;
    float release_speed = 0.0001f;
    
    if (abs_input > env_l) {
        env_l += (abs_input - env_l) * attack_speed;
    } else {
        env_l += (abs_input - env_l) * release_speed;
    }
    
    if (env_l < 1e-15f) env_l = 0.f;
    
    // Shape based on attack parameter
    float shape = 0.5f + s_attack * 0.5f;
    
    return input * (shape + (1.f - shape) * env_l);
}

// ========== MAIN PROCESSOR ==========

inline void process_timbre_kutchanger(float in_l, float in_r, float *out_l, float *out_r) {
    // Input validation
    if (!is_finite(in_l)) in_l = 0.f;
    if (!is_finite(in_r)) in_r = 0.f;
    
    // Apply vibrato
    float vibrato = get_vibrato();
    float mod_l = in_l * (1.f + vibrato);
    float mod_r = in_r * (1.f + vibrato);
    
    // Process through character-specific formants
    float processed_l = 0.f, processed_r = 0.f;
    
    CharacterType char_type = get_character_type();
    
    switch (char_type) {
        case CHAR_ELECTRIC:
            processed_l = process_electric(mod_l, s_formant_l);
            processed_r = process_electric(mod_r, s_formant_r);
            break;
            
        case CHAR_METALLIC:
            processed_l = process_metallic(mod_l, s_formant_l);
            processed_r = process_metallic(mod_r, s_formant_r);
            break;
            
        case CHAR_FLUTE:
            processed_l = process_flute(mod_l, s_formant_l);
            processed_r = process_flute(mod_r, s_formant_r);
            break;
            
        case CHAR_VOCAL_LOW:
            processed_l = process_vocal_low(mod_l, s_formant_l);
            processed_r = process_vocal_low(mod_r, s_formant_r);
            break;
            
        case CHAR_VOCAL_HIGH:
            processed_l = process_vocal_high(mod_l, s_formant_l);
            processed_r = process_vocal_high(mod_r, s_formant_r);
            break;
    }
    
    // Apply ensemble
    processed_l = process_ensemble(processed_l, &s_ensemble_phase_l, 0.f);
    processed_r = process_ensemble(processed_r, &s_ensemble_phase_r, 0.2f);
    
    // Apply harmonic emphasis
    processed_l = apply_harmonic_emphasis(processed_l);
    processed_r = apply_harmonic_emphasis(processed_r);
    
    // Apply attack shaping
    processed_l = apply_attack(processed_l);
    processed_r = apply_attack(processed_r);
    
    // Validate
    if (!is_finite(processed_l)) processed_l = 0.f;
    if (!is_finite(processed_r)) processed_r = 0.f;
    
    // Mix
    float mixed_l = in_l * (1.f - s_mix) + processed_l * s_mix;
    float mixed_r = in_r * (1.f - s_mix) + processed_r * s_mix;
    
    // Apply output gain boost (1.4x = +3dB to compensate for formant filtering losses)
    const float output_gain = 1.4f;
    *out_l = mixed_l * output_gain;
    *out_r = mixed_r * output_gain;
}

// ========== CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;
    
    // Allocate delay buffer (optional, for future chorus)
    if (desc->hooks.sdram_alloc) {
        size_t buffer_size = DELAY_BUFFER_SIZE * sizeof(float) * 2;
        uint8_t *buffer_base = static_cast<uint8_t *>(desc->hooks.sdram_alloc(buffer_size));
        
        if (buffer_base) {
            s_delay_buffer_l = reinterpret_cast<float *>(buffer_base);
            s_delay_buffer_r = reinterpret_cast<float *>(buffer_base + DELAY_BUFFER_SIZE * sizeof(float));
            
            for (uint32_t i = 0; i < DELAY_BUFFER_SIZE; i++) {
                s_delay_buffer_l[i] = 0.f;
                s_delay_buffer_r[i] = 0.f;
            }
        }
    }
    
    s_write_pos = 0;
    
    // Init formant filters
    for (int i = 0; i < 3; i++) {
        formant_init(&s_formant_l[i], 1000.f, 2.f);
        formant_init(&s_formant_r[i], 1000.f, 2.f);
    }
    
    // Init LFO
    s_vibrato_phase = 0.f;
    s_ensemble_phase_l = 0.f;
    s_ensemble_phase_r = 0.25f;
    
    // Init parameters
    s_character = 0.5f;
    s_brightness = 0.5f;
    s_formant = 0.4f;
    s_motion = 0.3f;
    s_ensemble = 0.5f;
    s_harmonic = 0.5f;
    s_attack = 0.5f;
    s_mix = 0.75f;
    s_color = 0.5f;
    s_depth = 0.6f;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    for (int i = 0; i < 3; i++) {
        s_formant_l[i].z1 = 0.f;
        s_formant_l[i].z2 = 0.f;
        s_formant_r[i].z1 = 0.f;
        s_formant_r[i].z2 = 0.f;
    }
    
    s_vibrato_phase = 0.f;
    s_ensemble_phase_l = 0.f;
    s_ensemble_phase_r = 0.25f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    const float *in_ptr = in;
    float *out_ptr = out;
    
    for (uint32_t f = 0; f < frames; f++) {
        float out_l, out_r;
        process_timbre_kutchanger(in_ptr[0], in_ptr[1], &out_l, &out_r);
        
        // Final clipping with safety margin
        out_ptr[0] = clipminmaxf(-1.f, out_l, 1.f);
        out_ptr[1] = clipminmaxf(-1.f, out_r, 1.f);
        
        in_ptr += 2;
        out_ptr += 2;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_character = valf; break;
        case 1: s_brightness = valf; break;
        case 2: s_formant = valf; break;
        case 3: s_motion = valf; break;
        case 4: s_ensemble = valf; break;
        case 5: s_harmonic = valf; break;
        case 6: s_attack = valf; break;
        case 7: s_mix = valf; break;
        case 8: s_color = valf; break;
        case 9: s_depth = valf; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)(s_character * 1023.f);
        case 1: return (int32_t)(s_brightness * 1023.f);
        case 2: return (int32_t)(s_formant * 1023.f);
        case 3: return (int32_t)(s_motion * 1023.f);
        case 4: return (int32_t)(s_ensemble * 1023.f);
        case 5: return (int32_t)(s_harmonic * 1023.f);
        case 6: return (int32_t)(s_attack * 1023.f);
        case 7: return (int32_t)(s_mix * 1023.f);
        case 8: return (int32_t)(s_color * 1023.f);
        case 9: return (int32_t)(s_depth * 1023.f);
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

