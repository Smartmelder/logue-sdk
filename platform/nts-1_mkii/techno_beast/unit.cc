/*
    TECHNO BEAST - Implementation (HOUSE EDITION!)
    
    CHANGES:
    - Noise parameter REMOVED (caused crackling)
    - Added ACCENT parameter (velocity sensitivity)
    - Added GLIDE parameter (portamento/slide)
    
    The ultimate techno/house oscillator for NTS-1 mkII
*/

#include "unit_osc.h"
#include "osc_api.h"
#include "fx_api.h"  // For fx_pow2f()
#include "utils/float_math.h"
#include "utils/int_math.h"

// ========== OSCILLATOR MODES ==========
enum OscMode {
    MODE_UNISON_SAW = 0,  // 7-voice supersaw
    MODE_UNISON_SQR = 1,  // 5-voice supersquare
    MODE_OCTAVE_SAW = 2,  // 3-octave saw stack
    MODE_OCTAVE_SQR = 3   // 3-octave square stack
};

const char* mode_names[4] = {
    "UNISAW",
    "UNISQR", 
    "OCTSAW",
    "OCTSQR"
};

// ========== VOICE STRUCTURE ==========
#define MAX_UNISON_VOICES 7

struct Voice {
    float phase[MAX_UNISON_VOICES];
    float sub_phase_1;  // -1 octave
    float sub_phase_2;  // -2 octaves
    float sync_phase;   // Master phase for sync
    float pwm_phase;    // PWM LFO phase
    float w0;           // Current frequency
    float w0_target;    // Target frequency (for glide)
    float filter_z1;    // Filter state 1
    float filter_z2;    // Filter state 2
    float velocity;     // Current note velocity
    bool active;
};

static Voice s_voice;

// ========== PARAMETERS ==========
static uint8_t s_mode = MODE_UNISON_SAW;
static float s_detune = 0.6f;         // 60%
static float s_sub_mix = 0.4f;        // 40%
static float s_sync_amount = 0.0f;    // 0%
static float s_pwm_depth = 0.3f;      // 30%
static float s_filter_cutoff = 0.8f;  // 80%
static float s_filter_resonance = 0.3f; // 30%
static float s_drive = 0.2f;          // 20%
static float s_accent = 0.5f;           // ✅ NEW: Velocity sensitivity
static float s_glide = 0.0f;            // ✅ NEW: Portamento time
static float s_phase_spread = 0.0f;

// ========== POLY BLEP ANTI-ALIASING ==========
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

// ========== PWM LFO ==========
inline float get_pwm_offset() {
    // Sine wave LFO at ~2 Hz
    float lfo = osc_sinf(s_voice.pwm_phase);
    s_voice.pwm_phase += 2.0f / 48000.f;  // 2 Hz
    if (s_voice.pwm_phase >= 1.f) s_voice.pwm_phase -= 1.f;
    
    // PWM range: 0.2 to 0.8 (avoid extremes)
    return 0.5f + lfo * s_pwm_depth * 0.3f;
}

// ========== GLIDE (PORTAMENTO) ==========
inline void process_glide() {
    if (s_glide < 0.01f) {
        // No glide: instant pitch change
        s_voice.w0 = s_voice.w0_target;
        return;
    }
    
    // Calculate glide rate (0.001 to 0.1)
    // Lower = slower glide
    float glide_rate = 0.001f + (1.f - s_glide) * 0.099f;
    
    // Exponential glide towards target
    if (si_fabsf(s_voice.w0 - s_voice.w0_target) > 0.0001f) {
        s_voice.w0 += (s_voice.w0_target - s_voice.w0) * glide_rate;
    } else {
        s_voice.w0 = s_voice.w0_target;
    }
}

// ========== ACCENT (VELOCITY RESPONSE) ==========
inline float get_accent_gain() {
    if (s_accent < 0.01f) {
        return 1.0f;  // No accent: all notes same volume
    }
    
    // Map velocity (0-127) to gain (0.3-1.2)
    float vel_normalized = s_voice.velocity / 127.f;
    
    // Apply accent curve
    float accent_amount = s_accent;
    float min_gain = 0.3f + (1.f - accent_amount) * 0.4f;  // 0.3-0.7
    float max_gain = 1.0f + accent_amount * 0.5f;          // 1.0-1.5
    
    float gain = min_gain + vel_normalized * (max_gain - min_gain);
    return clipminmaxf(0.3f, gain, 1.5f);
}

// ========== HARD SYNC ==========
inline float apply_hard_sync(float phase, float sync_phase, float sync_amount) {
    if (sync_amount < 0.01f) return phase;
    
    // Sync oscillator runs faster
    float sync_ratio = 1.f + sync_amount * 3.f;  // 1× to 4× faster
    float sync_p = sync_phase * sync_ratio;
    while (sync_p >= 1.f) sync_p -= 1.f;
    
    // Reset phase when sync crosses zero
    if (sync_p < phase) {
        return sync_p;
    }
    
    return phase;
}

// ========== SAWTOOTH GENERATOR ==========
inline float generate_saw(float phase, float w) {
    float saw = 2.f * phase - 1.f;
    saw -= poly_blep(phase, w);
    return saw;
}

// ========== SQUARE GENERATOR ==========
inline float generate_square(float phase, float w, float pulse_width) {
    float square = (phase < pulse_width) ? 1.f : -1.f;
    
    // PolyBLEP at rising edge
    square += poly_blep(phase, w);
    
    // PolyBLEP at falling edge
    float phase_shifted = phase + (1.f - pulse_width);
    if (phase_shifted >= 1.f) phase_shifted -= 1.f;
    square -= poly_blep(phase_shifted, w);
    
    return square;
}

// ========== UNISON SAW (7 voices) ==========
inline float generate_unison_saw() {
    float sum = 0.f;
    const int num_voices = 7;
    
    for (int v = 0; v < num_voices; v++) {
        // Detune spread: center voice = 0, others spread out
        float detune_cents = 0.f;
        if (v != num_voices / 2) {  // Center voice = no detune
            int offset = v - num_voices / 2;
            detune_cents = (float)offset * s_detune * 12.f;  // ±36 cents max
        }
        
        float ratio = fx_pow2f(detune_cents / 1200.f);
        float w = s_voice.w0 * ratio;
        w = clipminmaxf(0.0001f, w, 0.48f);
        
        // ✅ NEW: Apply phase spread
        float p = s_voice.phase[v] + (float)v * s_phase_spread * 0.14f;
        while (p >= 1.f) p -= 1.f;
        
        // Apply hard sync
        p = apply_hard_sync(p, s_voice.sync_phase, s_sync_amount);
        
        // Generate saw
        float saw = generate_saw(p, w);
        sum += saw;
        
        // Advance phase
        s_voice.phase[v] += w;
        if (s_voice.phase[v] >= 1.f) s_voice.phase[v] -= 1.f;
    }
    
    return sum / (float)num_voices;
}

// ========== UNISON SQUARE (5 voices) ==========
inline float generate_unison_square() {
    float sum = 0.f;
    const int num_voices = 5;
    float pwm = get_pwm_offset();
    
    for (int v = 0; v < num_voices; v++) {
        // Detune spread
        float detune_cents = 0.f;
        if (v != num_voices / 2) {
            int offset = v - num_voices / 2;
            detune_cents = (float)offset * s_detune * 10.f;  // ±20 cents max
        }
        
        float ratio = fx_pow2f(detune_cents / 1200.f);
        float w = s_voice.w0 * ratio;
        w = clipminmaxf(0.0001f, w, 0.48f);
        
        // ✅ NEW: Apply phase spread
        float p = s_voice.phase[v] + (float)v * s_phase_spread * 0.2f;
        while (p >= 1.f) p -= 1.f;
        
        // Apply hard sync
        p = apply_hard_sync(p, s_voice.sync_phase, s_sync_amount);
        
        // Generate square with PWM
        float square = generate_square(p, w, pwm);
        sum += square;
        
        // Advance phase
        s_voice.phase[v] += w;
        if (s_voice.phase[v] >= 1.f) s_voice.phase[v] -= 1.f;
    }
    
    return sum / (float)num_voices;
}

// ========== OCTAVE DOUBLING SAW ==========
inline float generate_octave_saw() {
    float sum = 0.f;
    
    // Base octave (100%)
    float p0 = s_voice.phase[0];
    p0 = apply_hard_sync(p0, s_voice.sync_phase, s_sync_amount);
    float saw0 = generate_saw(p0, s_voice.w0);
    sum += saw0 * 1.0f;
    
    // +1 octave (80%)
    float p1 = s_voice.phase[1];
    float w1 = s_voice.w0 * 2.f;
    w1 = clipminmaxf(0.0001f, w1, 0.48f);
    float saw1 = generate_saw(p1, w1);
    sum += saw1 * 0.8f;
    
    // +2 octaves (60%)
    float p2 = s_voice.phase[2];
    float w2 = s_voice.w0 * 4.f;
    w2 = clipminmaxf(0.0001f, w2, 0.48f);
    float saw2 = generate_saw(p2, w2);
    sum += saw2 * 0.6f;
    
    // Advance phases
    s_voice.phase[0] += s_voice.w0;
    if (s_voice.phase[0] >= 1.f) s_voice.phase[0] -= 1.f;
    
    s_voice.phase[1] += w1;
    if (s_voice.phase[1] >= 1.f) s_voice.phase[1] -= 1.f;
    
    s_voice.phase[2] += w2;
    if (s_voice.phase[2] >= 1.f) s_voice.phase[2] -= 1.f;
    
    return sum / 2.4f;  // Normalize
}

// ========== OCTAVE DOUBLING SQUARE ==========
inline float generate_octave_square() {
    float sum = 0.f;
    float pwm = get_pwm_offset();
    
    // Base octave (100%)
    float p0 = s_voice.phase[0];
    p0 = apply_hard_sync(p0, s_voice.sync_phase, s_sync_amount);
    float sqr0 = generate_square(p0, s_voice.w0, pwm);
    sum += sqr0 * 1.0f;
    
    // +1 octave (80%)
    float p1 = s_voice.phase[1];
    float w1 = s_voice.w0 * 2.f;
    w1 = clipminmaxf(0.0001f, w1, 0.48f);
    float sqr1 = generate_square(p1, w1, pwm);
    sum += sqr1 * 0.8f;
    
    // +2 octaves (60%)
    float p2 = s_voice.phase[2];
    float w2 = s_voice.w0 * 4.f;
    w2 = clipminmaxf(0.0001f, w2, 0.48f);
    float sqr2 = generate_square(p2, w2, pwm);
    sum += sqr2 * 0.6f;
    
    // Advance phases
    s_voice.phase[0] += s_voice.w0;
    if (s_voice.phase[0] >= 1.f) s_voice.phase[0] -= 1.f;
    
    s_voice.phase[1] += w1;
    if (s_voice.phase[1] >= 1.f) s_voice.phase[1] -= 1.f;
    
    s_voice.phase[2] += w2;
    if (s_voice.phase[2] >= 1.f) s_voice.phase[2] -= 1.f;
    
    return sum / 2.4f;  // Normalize
}

// ========== SUB OSCILLATOR ==========
inline float generate_sub() {
    float sum = 0.f;
    
    // -1 octave (sine wave)
    float w_sub1 = s_voice.w0 * 0.5f;
    float sub1 = osc_sinf(s_voice.sub_phase_1);
    sum += sub1 * 0.6f;
    
    s_voice.sub_phase_1 += w_sub1;
    if (s_voice.sub_phase_1 >= 1.f) s_voice.sub_phase_1 -= 1.f;
    
    // -2 octaves (sine wave)
    float w_sub2 = s_voice.w0 * 0.25f;
    float sub2 = osc_sinf(s_voice.sub_phase_2);
    sum += sub2 * 0.4f;
    
    s_voice.sub_phase_2 += w_sub2;
    if (s_voice.sub_phase_2 >= 1.f) s_voice.sub_phase_2 -= 1.f;
    
    return sum;
}

// ========== STATE VARIABLE FILTER (FIXED!) ==========
inline float process_filter(float input) {
    // ✅ FIX: Safe cutoff range (100Hz - 12kHz instead of 18kHz)
    // Above 12kHz, SVF becomes unstable at 48kHz sample rate
    float cutoff_hz = 100.f + s_filter_cutoff * 11900.f;  // 100-12000 Hz
    cutoff_hz = clipminmaxf(100.f, cutoff_hz, 12000.f);
    
    // ✅ FIX: Proper frequency warping
    float w = 2.f * 3.14159265f * cutoff_hz / 48000.f;
    w = clipminmaxf(0.001f, w, 1.5f);  // Hard limit!
    
    // ✅ FIX: Safe f calculation (use si_sinf for oscillators)
    float phase_norm = w / (2.f * 3.14159265f);
    float f = 2.f * si_sinf(phase_norm * 0.5f);
    f = clipminmaxf(0.0001f, f, 1.9f);  // MAX 1.9 (safe!)
    
    // ✅ FIX: Resonance (safe range)
    float q = 1.f / (0.5f + s_filter_resonance * 1.5f);
    q = clipminmaxf(0.5f, q, 2.0f);  // MAX 2.0 (safer!)
    
    // SVF processing
    s_voice.filter_z2 += f * s_voice.filter_z1;
    float hp = input - s_voice.filter_z2 - q * s_voice.filter_z1;
    s_voice.filter_z1 += f * hp;
    
    // ✅ FIX: Aggressive denormal kill
    if (si_fabsf(s_voice.filter_z1) < 1e-10f) s_voice.filter_z1 = 0.f;
    if (si_fabsf(s_voice.filter_z2) < 1e-10f) s_voice.filter_z2 = 0.f;
    
    // ✅ FIX: Clip states (prevent explosion)
    s_voice.filter_z1 = clipminmaxf(-3.f, s_voice.filter_z1, 3.f);
    s_voice.filter_z2 = clipminmaxf(-3.f, s_voice.filter_z2, 3.f);
    
    return s_voice.filter_z2;  // Lowpass output
}

// ========== OVERDRIVE ==========
inline float apply_overdrive(float input) {
    if (s_drive < 0.01f) return input;
    
    // Soft clipping with drive
    float drive_amount = 1.f + s_drive * 4.f;  // 1× to 5× gain
    float driven = input * drive_amount;
    
    // Soft clip (tanh)
    driven = fastertanh2f(driven);
    
    return driven;
}

// ========== MAIN OSCILLATOR ==========
inline float generate_oscillator() {
    if (!s_voice.active) return 0.f;
    
    // ✅ Process glide
    process_glide();
    
    // Generate main oscillator based on mode
    float osc = 0.f;
    
    switch (s_mode) {
        case MODE_UNISON_SAW:
            osc = generate_unison_saw();
            break;
            
        case MODE_UNISON_SQR:
            osc = generate_unison_square();
            break;
            
        case MODE_OCTAVE_SAW:
            osc = generate_octave_saw();
            break;
            
        case MODE_OCTAVE_SQR:
            osc = generate_octave_square();
            break;
    }
    
    // Add sub oscillator
    float sub = generate_sub();
    osc = osc * (1.f - s_sub_mix) + sub * s_sub_mix;
    
    // Apply filter
    osc = process_filter(osc);
    
    // Apply overdrive
    osc = apply_overdrive(osc);
    
    // ✅ Apply accent (velocity)
    osc *= get_accent_gain();
    
    // Advance sync phase
    s_voice.sync_phase += s_voice.w0;
    if (s_voice.sync_phase >= 1.f) s_voice.sync_phase -= 1.f;
    
    return osc;
}

// ========== UNIT CALLBACKS ==========
__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    
    // Init voice
    s_voice.active = false;
    s_voice.w0 = 0.f;
    s_voice.w0_target = 0.f;
    s_voice.velocity = 100.f;
    s_voice.sync_phase = 0.f;
    s_voice.pwm_phase = 0.f;
    s_voice.sub_phase_1 = 0.f;
    s_voice.sub_phase_2 = 0.f;
    s_voice.filter_z1 = 0.f;
    s_voice.filter_z2 = 0.f;
    
    for (int i = 0; i < MAX_UNISON_VOICES; i++) {
        s_voice.phase[i] = 0.f;
    }
    
    // Init parameters
    s_mode = MODE_UNISON_SAW;
    s_detune = 0.6f;
    s_sub_mix = 0.4f;
    s_sync_amount = 0.0f;
    s_pwm_depth = 0.3f;
    s_filter_cutoff = 0.8f;
    s_filter_resonance = 0.3f;
    s_drive = 0.2f;
    s_accent = 0.5f;
    s_glide = 0.0f;
    s_phase_spread = 0.0f;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    s_voice.active = false;
    s_voice.filter_z1 = 0.f;
    s_voice.filter_z2 = 0.f;
}

__unit_callback void unit_resume() {}

__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    (void)in;
    
    for (uint32_t f = 0; f < frames; f++) {
        // Generate oscillator
        float sample = generate_oscillator();
        
        // Output gain
        sample *= 1.8f;
        
        // Hard limiting
        sample = clipminmaxf(-1.f, sample, 1.f);
        
        // Mono output (oscillators are mono)
        out[f] = sample;
    }
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
    // ✅ Store velocity for accent
    s_voice.velocity = (float)velocity;
    
    // Set target frequency (for glide)
    s_voice.w0_target = osc_w0f_for_note(note, 0);
    
    // If glide is off or first note, set frequency immediately
    if (s_glide < 0.01f || !s_voice.active) {
        s_voice.w0 = s_voice.w0_target;
        
        // Reset phases only on first note
        for (int i = 0; i < MAX_UNISON_VOICES; i++) {
            s_voice.phase[i] = 0.f;
        }
        s_voice.sync_phase = 0.f;
        s_voice.pwm_phase = 0.f;
        s_voice.sub_phase_1 = 0.f;
        s_voice.sub_phase_2 = 0.f;
        
        // Reset filter
        s_voice.filter_z1 = 0.f;
        s_voice.filter_z2 = 0.f;
    }
    // If glide is on and voice already active, phases continue
    
    s_voice.active = true;
}

__unit_callback void unit_note_off(uint8_t note) {
    (void)note;
    // Don't immediately turn off if glide is active
    // This allows smooth transitions between notes
    if (s_glide < 0.01f) {
        s_voice.active = false;
    }
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
        case 0: // Mode
            s_mode = (uint8_t)value;
            if (s_mode > 3) s_mode = 3;
            break;
            
        case 1: // Detune
            s_detune = valf;
            break;
            
        case 2: // Sub Mix
            s_sub_mix = valf;
            break;
            
        case 3: // Sync
            s_sync_amount = valf;
            break;
            
        case 4: // PWM
            s_pwm_depth = valf;
            break;
            
        case 5: // Filter
            s_filter_cutoff = valf;
            break;
            
        case 6: // Resonance
            s_filter_resonance = valf;
            break;
            
        case 7: // Drive
            s_drive = valf;
            break;
            
        case 8: // ✅ Accent (was Noise)
            s_accent = valf;
            break;
            
        case 9: // ✅ Glide (NEW!)
            s_glide = valf;
            break;
            
        case 10: // Phase Spread
            s_phase_spread = valf;
            break;
            
        default:
            break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return s_mode;
        case 1: return (int32_t)(s_detune * 1023.f);
        case 2: return (int32_t)(s_sub_mix * 1023.f);
        case 3: return (int32_t)(s_sync_amount * 1023.f);
        case 4: return (int32_t)(s_pwm_depth * 1023.f);
        case 5: return (int32_t)(s_filter_cutoff * 1023.f);
        case 6: return (int32_t)(s_filter_resonance * 1023.f);
        case 7: return (int32_t)(s_drive * 1023.f);
        case 8: return (int32_t)(s_accent * 1023.f);
        case 9: return (int32_t)(s_glide * 1023.f);
        case 10: return (int32_t)(s_phase_spread * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 0 && value >= 0 && value < 4) {
        return mode_names[value];
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    (void)tempo;
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    (void)counter;
}

