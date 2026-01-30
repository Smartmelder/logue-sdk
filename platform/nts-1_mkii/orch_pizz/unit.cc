/*
    ORCHESTRAL PIZZICATO - 90s Sampler Emulation

    A professional orchestral pluck synthesizer emulating classic samplers
    like the Roland JD-800, Fairlight CMI, and Akai S950.

    FEATURES:
    - SuperSaw oscillator engine (7 detuned saws per voice)
    - Parallel chord generation (Root + Fifth + Octave)
    - Internal amplitude envelope (ultra-fast attack, exponential decay)
    - Vintage 12-bit sampler character (bit crushing, sample rate reduction)
    - Multi-mode filter (LP/BP/HP)
    - Stereo spread and width control
    - Velocity sensitivity
    - 10 parameters for total control

    ARCHITECTURE:
    - 3 chord voices (Root, +7, +12 semitones)
    - Each voice has 7-voice SuperSaw unison
    - Total: 21 oscillators per note
    - Optimized for NTS-1 MKII (STM32H7)
*/

#include "unit_osc.h"
#include "fx_api.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include <algorithm>
#include <cmath>

// ═══════════════════════════════════════════════════════════════════════════
// CONSTANTS & TABLES
// ═══════════════════════════════════════════════════════════════════════════

#define NUM_VOICES 3          // Root, Fifth, Octave
#define NUM_UNISON 7          // SuperSaw voices per chord note
#define TOTAL_OSCS (NUM_VOICES * NUM_UNISON)  // 21 oscillators

// SuperSaw detune amounts (cents) for 7-voice unison
static const float s_unison_detune[NUM_UNISON] = {
    -12.0f, -8.0f, -4.0f, 0.0f, 4.0f, 8.0f, 12.0f
};

// Stereo spread positions for unison voices
static const float s_unison_pan[NUM_UNISON] = {
    -1.0f, -0.67f, -0.33f, 0.0f, 0.33f, 0.67f, 1.0f
};

// Chord intervals (semitones)
static const int8_t s_chord_intervals[NUM_VOICES] = {
    0,    // Root
    7,    // Perfect Fifth
    12    // Octave
};

// Default chord mix levels
static const float s_chord_mix[NUM_VOICES] = {
    1.0f,   // Root (always full)
    0.7f,   // Fifth (slightly quieter)
    0.5f    // Octave (background)
};

// ═══════════════════════════════════════════════════════════════════════════
// BIQUAD FILTER (Multi-mode: LP/BP/HP)
// ═══════════════════════════════════════════════════════════════════════════

struct BiquadFilter {
    float b0, b1, b2, a1, a2;
    float z1_l, z2_l, z1_r, z2_r;

    void reset() {
        z1_l = z2_l = z1_r = z2_r = 0.f;
    }

    void set_lowpass(float freq, float q) {
        freq = clipminmaxf(20.f, freq, 20000.f);
        q = clipminmaxf(0.5f, q, 20.f);

        float omega = 2.f * 3.14159265f * freq / 48000.f;
        float sn = fx_sinf(omega / (2.f * 3.14159265f));
        float cs = fx_cosf(omega / (2.f * 3.14159265f));
        float alpha = sn / (2.f * q);

        float a0 = 1.f + alpha;
        b0 = ((1.f - cs) / 2.f) / a0;
        b1 = (1.f - cs) / a0;
        b2 = ((1.f - cs) / 2.f) / a0;
        a1 = (-2.f * cs) / a0;
        a2 = (1.f - alpha) / a0;
    }

    void set_bandpass(float freq, float q) {
        freq = clipminmaxf(20.f, freq, 20000.f);
        q = clipminmaxf(0.5f, q, 20.f);

        float omega = 2.f * 3.14159265f * freq / 48000.f;
        float sn = fx_sinf(omega / (2.f * 3.14159265f));
        float cs = fx_cosf(omega / (2.f * 3.14159265f));
        float alpha = sn / (2.f * q);

        float a0 = 1.f + alpha;
        b0 = alpha / a0;
        b1 = 0.f;
        b2 = -alpha / a0;
        a1 = (-2.f * cs) / a0;
        a2 = (1.f - alpha) / a0;
    }

    void set_highpass(float freq, float q) {
        freq = clipminmaxf(20.f, freq, 20000.f);
        q = clipminmaxf(0.5f, q, 20.f);

        float omega = 2.f * 3.14159265f * freq / 48000.f;
        float sn = fx_sinf(omega / (2.f * 3.14159265f));
        float cs = fx_cosf(omega / (2.f * 3.14159265f));
        float alpha = sn / (2.f * q);

        float a0 = 1.f + alpha;
        b0 = ((1.f + cs) / 2.f) / a0;
        b1 = (-(1.f + cs)) / a0;
        b2 = ((1.f + cs) / 2.f) / a0;
        a1 = (-2.f * cs) / a0;
        a2 = (1.f - alpha) / a0;
    }

    inline void process_stereo(float *l, float *r) {
        // Left channel
        float out_l = b0 * (*l) + b1 * z1_l + b2 * z2_l - a1 * z1_l - a2 * z2_l;
        z2_l = z1_l;
        z1_l = *l;

        // Right channel
        float out_r = b0 * (*r) + b1 * z1_r + b2 * z2_r - a1 * z1_r - a2 * z2_r;
        z2_r = z1_r;
        z1_r = *r;

        // Anti-denormal
        if (si_fabsf(out_l) < 1e-15f) out_l = 0.f;
        if (si_fabsf(out_r) < 1e-15f) out_r = 0.f;

        *l = clipminmaxf(-2.f, out_l, 2.f);
        *r = clipminmaxf(-2.f, out_r, 2.f);
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// AMPLITUDE ENVELOPE (Internal AR envelope)
// ═══════════════════════════════════════════════════════════════════════════

struct AmplitudeEnvelope {
    float phase;
    float attack_rate;
    float decay_rate;
    bool active;

    void init() {
        phase = 0.f;
        attack_rate = 1000.f;  // Ultra-fast attack (1ms)
        decay_rate = 0.995f;   // Exponential decay
        active = false;
    }

    void trigger() {
        phase = 1.f;  // Start at peak for instant attack
        active = true;
    }

    void set_decay(float decay_time) {
        // Decay time in seconds (0.05 - 5.0)
        decay_time = clipminmaxf(0.05f, decay_time, 5.0f);
        // Calculate decay coefficient for exponential curve
        // Formula: decay_rate = exp(-1 / (decay_time * samplerate))
        // Simplified: decay_rate ≈ 1 - (constant / (decay_time * samplerate))
        float samples = decay_time * 48000.f;
        decay_rate = 1.f - (6.9078f / samples);  // ln(1000) ≈ 6.9078
        decay_rate = clipminmaxf(0.9f, decay_rate, 0.9999f);
    }

    inline float process() {
        if (!active) return 0.f;

        // Decay phase (exponential from peak)
        phase *= decay_rate;

        // Stop when very quiet
        if (phase < 0.0001f) {
            active = false;
            phase = 0.f;
            return 0.f;
        }

        return phase;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// SUPERSAW OSCILLATOR
// ═══════════════════════════════════════════════════════════════════════════

struct SuperSawOsc {
    float phase[TOTAL_OSCS];
    float base_freq;

    void init() {
        for (int i = 0; i < TOTAL_OSCS; i++) {
            phase[i] = 0.f;
        }
        base_freq = 440.f;
    }

    void note_on(float freq) {
        base_freq = freq;
        // Randomize phases for wider sound
        for (int i = 0; i < TOTAL_OSCS; i++) {
            phase[i] = (float)(i * 137) / (float)TOTAL_OSCS;  // Golden ratio distribution
        }
    }

    inline void process_stereo(float *out_l, float *out_r,
                               float detune_amt, float chord_balance,
                               float stereo_width) {
        float sum_l = 0.f;
        float sum_r = 0.f;

        int osc_idx = 0;

        // Process 3 chord voices
        for (int voice = 0; voice < NUM_VOICES; voice++) {
            // Calculate chord frequency
            float semitone_shift = (float)s_chord_intervals[voice];
            float chord_freq = base_freq * fx_pow2f(semitone_shift / 12.f);

            // Voice volume based on chord balance
            float voice_vol = s_chord_mix[voice];
            if (voice > 0) {
                voice_vol *= chord_balance;  // Fade fifth and octave
            }

            // Process unison voices
            for (int unison = 0; unison < NUM_UNISON; unison++) {
                // Calculate detuned frequency
                float detune_cents = s_unison_detune[unison] * detune_amt;
                float detune_ratio = fx_pow2f(detune_cents / 1200.f);
                float osc_freq = chord_freq * detune_ratio;

                // Phase increment
                float phase_inc = osc_freq / 48000.f;
                phase[osc_idx] += phase_inc;
                if (phase[osc_idx] >= 1.f) phase[osc_idx] -= 1.f;

                // Generate sawtooth wave
                float saw = (phase[osc_idx] * 2.f) - 1.f;

                // Apply voice volume
                saw *= voice_vol / (float)NUM_UNISON;

                // Stereo spread
                float pan = s_unison_pan[unison] * stereo_width;
                float pan_l = (1.f - pan) * 0.5f;
                float pan_r = (1.f + pan) * 0.5f;

                sum_l += saw * pan_l;
                sum_r += saw * pan_r;

                osc_idx++;
            }
        }

        // Normalize
        *out_l = sum_l * 0.3f;  // Scale down to prevent clipping
        *out_r = sum_r * 0.3f;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// VINTAGE DEGRADATION (Bit Crushing + Sample Rate Reduction)
// ═══════════════════════════════════════════════════════════════════════════

struct VintageDegrader {
    float hold_l, hold_r;
    uint32_t counter;
    uint32_t hold_period;

    void init() {
        hold_l = hold_r = 0.f;
        counter = 0;
        hold_period = 1;
    }

    void set_sample_rate_reduction(float amount) {
        // Amount 0-1: 0 = 48kHz, 1 = ~6kHz
        amount = clipminmaxf(0.f, amount, 1.f);
        hold_period = 1 + (uint32_t)(amount * 7.f);
    }

    inline void process_stereo(float *l, float *r, float bit_depth) {
        // Sample rate reduction (sample & hold)
        counter++;
        if (counter >= hold_period) {
            counter = 0;
            hold_l = *l;
            hold_r = *r;
        }

        // Bit depth reduction
        if (bit_depth < 1.f) {
            float levels = 4.f + bit_depth * 65532.f;  // 4-bit to 16-bit
            hold_l = floorf(hold_l * levels + 0.5f) / levels;
            hold_r = floorf(hold_r * levels + 0.5f) / levels;
        }

        *l = hold_l;
        *r = hold_r;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// GLOBAL STATE
// ═══════════════════════════════════════════════════════════════════════════

static SuperSawOsc s_osc;
static AmplitudeEnvelope s_env;
static BiquadFilter s_filter;
static VintageDegrader s_degrader;

// Parameters
static float s_decay_time;        // Pluck decay (0.05 - 5 sec)
static float s_detune_amount;     // SuperSaw spread (0 - 1)
static float s_chord_balance;     // Fifth/Octave mix (0 - 1)
static float s_filter_cutoff;     // Filter frequency
static float s_filter_reso;       // Filter resonance
static float s_bit_crush;         // Bit depth reduction
static float s_sample_rate_red;   // Sample rate reduction
static float s_stereo_width;      // Stereo spread
static float s_velocity_sens;     // Velocity sensitivity
static uint8_t s_filter_mode;     // 0=LP, 1=BP, 2=HP

static float s_velocity;          // Current note velocity
static bool s_note_active;

// ═══════════════════════════════════════════════════════════════════════════
// UNIT CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;

    s_osc.init();
    s_env.init();
    s_filter.reset();
    s_degrader.init();

    // Default parameters - Classic orchestral pizzicato
    s_decay_time = 1.0f;          // 1 second decay (was too short!)
    s_detune_amount = 0.6f;       // 60% SuperSaw spread
    s_chord_balance = 0.7f;       // 70% fifth/octave
    s_filter_cutoff = 0.75f;      // 75% cutoff (bright)
    s_filter_reso = 0.3f;         // 30% resonance
    s_bit_crush = 0.75f;          // 12-bit simulation
    s_sample_rate_red = 0.2f;     // Subtle SR reduction
    s_stereo_width = 0.8f;        // 80% stereo spread
    s_velocity_sens = 0.5f;       // 50% velocity sensitivity
    s_filter_mode = 0;            // Lowpass

    s_velocity = 1.0f;
    s_note_active = false;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    s_osc.init();
    s_env.init();
    s_filter.reset();
    s_degrader.init();
    s_note_active = false;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float * in, float * out, uint32_t frames)
{
    const float * __restrict in_p = in;
    float * __restrict out_p = out;

    // Update envelope decay
    s_env.set_decay(s_decay_time);

    // Update filter
    float cutoff_hz = 100.f + s_filter_cutoff * 11900.f;  // 100Hz - 12kHz
    float reso = 0.7f + s_filter_reso * 15.f;

    switch (s_filter_mode) {
        case 0:  s_filter.set_lowpass(cutoff_hz, reso); break;
        case 1:  s_filter.set_bandpass(cutoff_hz, reso); break;
        case 2:  s_filter.set_highpass(cutoff_hz, reso); break;
        default: s_filter.set_lowpass(cutoff_hz, reso); break;
    }

    // Update degrader
    s_degrader.set_sample_rate_reduction(s_sample_rate_red);

    for (uint32_t i = 0; i < frames; i++) {
        float out_l = 0.f;
        float out_r = 0.f;

        if (s_note_active) {
            // Get envelope value
            float env = s_env.process();

            // Apply velocity
            float vel_mod = 1.f - s_velocity_sens + (s_velocity * s_velocity_sens);
            env *= vel_mod;

            if (env > 0.f) {
                // Generate SuperSaw
                s_osc.process_stereo(&out_l, &out_r,
                                    s_detune_amount,
                                    s_chord_balance,
                                    s_stereo_width);

                // Apply envelope
                out_l *= env;
                out_r *= env;

                // Apply filter
                s_filter.process_stereo(&out_l, &out_r);

                // Apply vintage degradation
                s_degrader.process_stereo(&out_l, &out_r, s_bit_crush);

                // Soft clip
                out_l = fastertanhf(out_l * 0.9f);
                out_r = fastertanhf(out_r * 0.9f);
            } else {
                s_note_active = false;
            }
        }

        // Output
        out_p[i * 2] = clipminmaxf(-1.f, out_l, 1.f);
        out_p[i * 2 + 1] = clipminmaxf(-1.f, out_r, 1.f);
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    const float valf = param_val_to_f32(value);

    switch (id) {
        case 0:  // Decay time
            s_decay_time = 0.05f + valf * 4.95f;  // 50ms - 5 sec
            break;
        case 1:  // Detune amount
            s_detune_amount = valf;
            break;
        case 2:  // Chord balance
            s_chord_balance = valf;
            break;
        case 3:  // Filter cutoff
            s_filter_cutoff = valf;
            break;
        case 4:  // Filter resonance
            s_filter_reso = valf;
            break;
        case 5:  // Bit crush
            s_bit_crush = valf;
            break;
        case 6:  // Sample rate reduction
            s_sample_rate_red = valf;
            break;
        case 7:  // Stereo width
            s_stereo_width = valf;
            break;
        case 8:  // Velocity sensitivity
            s_velocity_sens = valf;
            break;
        case 9:  // Filter mode
            s_filter_mode = clipminmaxi32(0, value, 2);
            break;
        default:
            break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)((s_decay_time - 0.05f) / 4.95f * 1023.f);
        case 1: return (int32_t)(s_detune_amount * 1023.f);
        case 2: return (int32_t)(s_chord_balance * 1023.f);
        case 3: return (int32_t)(s_filter_cutoff * 1023.f);
        case 4: return (int32_t)(s_filter_reso * 1023.f);
        case 5: return (int32_t)(s_bit_crush * 1023.f);
        case 6: return (int32_t)(s_sample_rate_red * 1023.f);
        case 7: return (int32_t)(s_stereo_width * 1023.f);
        case 8: return (int32_t)(s_velocity_sens * 1023.f);
        case 9: return s_filter_mode;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 9) {
        static const char *filter_modes[] = {"LOWPASS", "BANDPASS", "HIPASS"};
        if (value >= 0 && value < 3) return filter_modes[value];
    }
    return "";
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity)
{
    // Calculate frequency from MIDI note
    float freq = 440.f * fx_pow2f(((float)note - 69.f) / 12.f);

    // Store velocity (0-127 -> 0-1)
    s_velocity = (float)velocity / 127.f;

    // Trigger oscillator and envelope
    s_osc.note_on(freq);
    s_env.trigger();
    s_note_active = true;
}

__unit_callback void unit_note_off(uint8_t note)
{
    (void)note;
    // Note: We don't stop on note_off - the envelope handles the decay
}

__unit_callback void unit_all_note_off()
{
    s_note_active = false;
    s_env.active = false;
}

__unit_callback void unit_pitch_bend(uint16_t bend)
{
    (void)bend;
    // Could implement pitch bend here if needed
}

__unit_callback void unit_channel_pressure(uint8_t pressure)
{
    (void)pressure;
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch)
{
    (void)note;
    (void)aftertouch;
}
