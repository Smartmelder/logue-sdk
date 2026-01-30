/*
    ELECTRIBE V3 - COMPLETE FIX!
    
    CRITICAL FIXES:
    - Removed static variable bug in hard sync
    - Proper hard sync implementation
    - Better oscillator architecture
    - NaN/Inf protection everywhere
    - Safe waveshaping
    
    REAL ELECTRIBE FEATURES:
    - Hard sync (proper implementation!)
    - Ring modulation
    - FM synthesis
    - Waveshaping
*/

#include "unit_osc.h"
#include "osc_api.h"
#include "utils/float_math.h"
#include "fx_api.h"

// ========== FAST TANH ==========
inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// ========== NaN/Inf CHECK MACRO ==========
#define is_finite(x) ((x) == (x) && (x) <= 1e10f && (x) >= -1e10f)

// ========== ELECTRIBE MODES ==========

enum ElectribeMode {
    MODE_SYNC = 0,
    MODE_RING,
    MODE_FM,
    MODE_WAVE
};

// ========== VOICE STATE ==========

struct Voice {
    float phase_master;
    float phase_slave;
    float phase_mod;
    float phase_detune1;
    float phase_detune2;
    
    float w0;
    
    float attack_env;
    float mod_env;
    
    // ✅ FIX: Store previous phase HERE, not static!
    float prev_master_phase;
    
    bool active;
};

static Voice s_voice;

// ========== PARAMETERS ==========

static ElectribeMode s_mode = MODE_SYNC;
static float s_character = 0.5f;
static float s_mod_amount = 0.3f;
static float s_harmonics = 0.6f;
static float s_attack = 0.5f;
static float s_brightness = 0.5f;
static float s_unison = 0.3f;
static float s_drive = 0.4f;
static float s_shape = 0.5f;
static float s_punch = 0.4f;

// ========== NOISE ==========

static uint32_t s_noise_state = 54321;

inline float noise() {
    s_noise_state = s_noise_state * 1103515245 + 12345;
    return ((s_noise_state >> 16) & 0x7FFF) / 16384.f - 1.f;
}

// ========== BASIC WAVEFORMS ==========

inline float osc_saw(float phase) {
    return 2.f * phase - 1.f;
}

inline float osc_tri(float phase) {
    return (phase < 0.5f) ? (4.f * phase - 1.f) : (3.f - 4.f * phase);
}

// ========== HARD SYNC (FIXED!) ==========

inline float generate_hard_sync(float sync_ratio) {
    // Master oscillator (resets slave)
    float master = osc_saw(s_voice.phase_master);
    
    // Slave frequency
    float slave_freq = 1.f + sync_ratio * 7.f;
    
    // ✅ FIX: Detect zero crossing properly
    bool crossed_zero = (s_voice.phase_master < s_voice.prev_master_phase);
    s_voice.prev_master_phase = s_voice.phase_master;
    
    // ✅ FIX: Only reset slave on actual zero crossing
    if (crossed_zero) {
        s_voice.phase_slave = 0.f;
    }
    
    // Generate slave saw
    float slave = osc_saw(s_voice.phase_slave);
    
    // Update slave phase
    s_voice.phase_slave += s_voice.w0 * slave_freq;
    if (s_voice.phase_slave >= 1.f) s_voice.phase_slave -= 1.f;
    
    // Mix master + slave
    return master * 0.3f + slave * 0.7f;
}

// ========== RING MODULATION ==========

inline float generate_ring_mod(float mod_amount) {
    float carrier = osc_saw(s_voice.phase_master);
    
    // Modulator frequency
    float mod_ratio = 0.5f + s_character * 7.5f;
    
    float mod = osc_sinf(s_voice.phase_mod * mod_ratio);
    
    // Ring mod
    float ring = carrier * mod;
    
    return carrier * (1.f - mod_amount * 0.7f) + ring * mod_amount;
}

// ========== FM SYNTHESIS ==========

inline float generate_fm(float fm_amount) {
    float mod_ratio = 1.f + s_character * 7.f;
    
    float modulator = osc_sinf(s_voice.phase_mod * mod_ratio);
    
    float mod_index = fm_amount * 5.f;
    
    float fm_phase = s_voice.phase_master + modulator * mod_index * 0.1f;
    while (fm_phase >= 1.f) fm_phase -= 1.f;
    while (fm_phase < 0.f) fm_phase += 1.f;
    
    return osc_sinf(fm_phase);
}

// ========== WAVESHAPING ==========

inline float generate_waveshape() {
    float saw = osc_saw(s_voice.phase_master);
    
    // Soft fold
    float fold = fast_tanh(saw * (1.f + s_character * 4.f));
    
    // Bit crush
    float bits = 8.f + (1.f - s_character) * 8.f;
    float scale = fx_pow2f(bits);
    float crush = si_floorf(saw * scale + 0.5f) / scale;
    
    return saw * (1.f - s_character * 0.5f) + fold * s_character * 0.3f + crush * s_character * 0.2f;
}

// ========== UNISON ==========

inline float apply_unison(float base) {
    if (s_unison < 0.01f) return base;
    
    float det1 = osc_saw(s_voice.phase_detune1);
    float det2 = osc_saw(s_voice.phase_detune2);
    
    float total = base + det1 * s_unison * 0.4f + det2 * s_unison * 0.4f;
    return total / (1.f + s_unison * 0.8f);
}

// ========== HARMONICS ==========

inline float add_harmonics(float base) {
    if (s_harmonics < 0.01f) return base;
    
    float h2 = osc_sinf(s_voice.phase_master * 2.f) * 0.3f;
    float h3 = osc_sinf(s_voice.phase_master * 3.f) * 0.2f;
    float h4 = osc_sinf(s_voice.phase_master * 4.f) * 0.15f;
    
    return base + (h2 + h3 + h4) * s_harmonics;
}

// ========== DRIVE ==========

inline float apply_drive(float input) {
    if (s_drive < 0.01f) return input;
    
    float driven = fast_tanh(input * (1.f + s_drive * 3.f));
    return input * (1.f - s_drive * 0.6f) + driven * s_drive * 0.6f;
}

// ========== BRIGHTNESS ==========

inline float apply_brightness(float input) {
    static float bright_z1 = 0.f;
    
    float coeff = 0.3f + s_brightness * 0.6f;
    bright_z1 += coeff * (input - bright_z1);
    
    if (si_fabsf(bright_z1) < 1e-15f) bright_z1 = 0.f;
    
    float hp = input - bright_z1;
    return bright_z1 * (1.f - s_brightness * 0.4f) + (input + hp * 0.3f) * s_brightness;
}

// ========== ENVELOPES ==========

inline void update_attack_env() {
    float speed = 0.0001f + (1.f - s_attack) * 0.005f;
    
    if (s_voice.attack_env < 1.f) {
        s_voice.attack_env += 1.f / (speed * 48000.f);
        if (s_voice.attack_env > 1.f) s_voice.attack_env = 1.f;
    }
}

inline void update_mod_env() {
    if (s_voice.mod_env < 1.f) {
        s_voice.mod_env += 0.01f;
        if (s_voice.mod_env > 1.f) s_voice.mod_env = 1.f;
    } else {
        s_voice.mod_env -= 0.0002f;
        if (s_voice.mod_env < 0.3f) s_voice.mod_env = 0.3f;
    }
}

// ========== MAIN OSCILLATOR ==========

inline float generate_electribe() {
    if (!s_voice.active) return 0.f;
    
    update_attack_env();
    update_mod_env();
    
    float mod_amt = s_mod_amount * s_voice.mod_env;
    
    float output = 0.f;
    
    // Generate based on mode
    switch (s_mode) {
        case MODE_SYNC:
            output = generate_hard_sync(s_character);
            break;
        case MODE_RING:
            output = generate_ring_mod(mod_amt);
            break;
        case MODE_FM:
            output = generate_fm(mod_amt);
            break;
        case MODE_WAVE:
            output = generate_waveshape();
            break;
    }
    
    // ✅ Validate after generation
    if (!is_finite(output)) output = 0.f;
    
    // Apply unison
    output = apply_unison(output);
    
    // Add harmonics
    output = add_harmonics(output);
    
    // Apply drive
    output = apply_drive(output);
    
    // Apply brightness
    output = apply_brightness(output);
    
    // Attack envelope + punch
    if (s_voice.attack_env < 1.f) {
        float click = (1.f - s_voice.attack_env) * s_punch * 0.2f;
        output += click * noise();
    }
    
    output *= s_voice.attack_env;
    
    // ✅ Final validation
    if (!is_finite(output)) output = 0.f;
    
    // Update phases
    s_voice.phase_master += s_voice.w0;
    if (s_voice.phase_master >= 1.f) s_voice.phase_master -= 1.f;
    
    s_voice.phase_mod += s_voice.w0;
    if (s_voice.phase_mod >= 1.f) s_voice.phase_mod -= 1.f;
    
    // Unison detune
    float detune1 = s_unison * 0.05f;
    float detune2 = -s_unison * 0.05f;
    
    float w0_det1 = s_voice.w0 * fx_pow2f(detune1 / 12.f);
    float w0_det2 = s_voice.w0 * fx_pow2f(detune2 / 12.f);
    
    s_voice.phase_detune1 += w0_det1;
    if (s_voice.phase_detune1 >= 1.f) s_voice.phase_detune1 -= 1.f;
    
    s_voice.phase_detune2 += w0_det2;
    if (s_voice.phase_detune2 >= 1.f) s_voice.phase_detune2 -= 1.f;
    
    return clipminmaxf(-1.f, output * 0.8f, 1.f);
}

// ========== CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 1) return k_unit_err_geometry;
    
    s_voice.active = false;
    s_voice.phase_master = 0.f;
    s_voice.phase_slave = 0.f;
    s_voice.phase_mod = 0.f;
    s_voice.phase_detune1 = 0.f;
    s_voice.phase_detune2 = 0.f;
    s_voice.w0 = 0.f;
    s_voice.attack_env = 0.f;
    s_voice.mod_env = 0.f;
    s_voice.prev_master_phase = 0.f;
    
    s_mode = MODE_SYNC;
    s_character = 0.5f;
    s_mod_amount = 0.3f;
    s_harmonics = 0.6f;
    s_attack = 0.5f;
    s_brightness = 0.5f;
    s_unison = 0.3f;
    s_drive = 0.4f;
    s_shape = 0.5f;
    s_punch = 0.4f;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    s_voice.active = false;
    s_voice.prev_master_phase = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    (void)in;
    
    for (uint32_t f = 0; f < frames; f++) {
        out[f] = generate_electribe();
    }
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
    (void)velocity;
    
    s_voice.phase_master = 0.f;
    s_voice.phase_slave = 0.f;
    s_voice.phase_mod = 0.f;
    s_voice.phase_detune1 = 0.1f;
    s_voice.phase_detune2 = 0.2f;
    s_voice.attack_env = 0.f;
    s_voice.mod_env = 0.f;
    s_voice.prev_master_phase = 0.f;
    
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
        case 0: // Mode
            if (valf < 0.25f) s_mode = MODE_SYNC;
            else if (valf < 0.5f) s_mode = MODE_RING;
            else if (valf < 0.75f) s_mode = MODE_FM;
            else s_mode = MODE_WAVE;
            break;
        case 1: s_character = valf; break;
        case 2: s_mod_amount = valf; break;
        case 3: s_harmonics = valf; break;
        case 4: s_attack = valf; break;
        case 5: s_brightness = valf; break;
        case 6: s_unison = valf; break;
        case 7: s_drive = valf; break;
        case 8: s_shape = valf; break;
        case 9: s_punch = valf; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: {
            float val = 0.f;
            switch (s_mode) {
                case MODE_SYNC: val = 0.125f; break;
                case MODE_RING: val = 0.375f; break;
                case MODE_FM: val = 0.625f; break;
                case MODE_WAVE: val = 0.875f; break;
            }
            return (int32_t)(val * 1023.f);
        }
        case 1: return (int32_t)(s_character * 1023.f);
        case 2: return (int32_t)(s_mod_amount * 1023.f);
        case 3: return (int32_t)(s_harmonics * 1023.f);
        case 4: return (int32_t)(s_attack * 1023.f);
        case 5: return (int32_t)(s_brightness * 1023.f);
        case 6: return (int32_t)(s_unison * 1023.f);
        case 7: return (int32_t)(s_drive * 1023.f);
        case 8: return (int32_t)(s_shape * 1023.f);
        case 9: return (int32_t)(s_punch * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 0) {
        switch (s_mode) {
            case MODE_SYNC: return "SYNC";
            case MODE_RING: return "RING";
            case MODE_FM: return "FM";
            case MODE_WAVE: return "WAVE";
        }
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    (void)tempo;
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    (void)counter;
}
