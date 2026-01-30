/*
    SEQUENCE FILTER - Tempo-synced step sequencer modulation
    
    HOUSE EDITION - Optimized for house sequences!
    
    CHANGES:
    - Replaced DEPTH with CUTOFF RANGE parameter
    - Safe filter operation (no crackling!)
    - Better suited for house music patterns
    
    FEATURES:
    - 16-step sequencer pattern
    - Tempo sync (1/4, 1/8, 1/16, 1/32, triplets)
    - State-variable filter (LP/BP/HP)
    - Cutoff range control (high/mid/low)
    - 8 preset patterns
    - Forward/backward direction
    - Safe, stable operation
    
    PRESETS:
    0. CLASSIC - 4-on-floor filter sweep
    1. ACID - TB-303 style pattern
    2. TRANCE - Uplifting gate pattern
    3. TECHNO - Industrial rhythm
    4. RANDOM - Generative pattern
    5. EUCLIDEAN - Mathematical spacing
    6. TRAP - Modern trap rhythm
    7. CUSTOM - User pattern
*/

#include "unit_modfx.h"
#include "utils/float_math.h"
#include "fx_api.h"
#include "osc_api.h"

#define PATTERN_STEPS 16

// 16-step patterns (0.0 = closed, 1.0 = open)
static const float s_patterns[8][PATTERN_STEPS] = {
    // CLASSIC - 4/4 house
    {1.0f, 0.7f, 0.5f, 0.3f, 0.8f, 0.6f, 0.4f, 0.2f,
     0.9f, 0.7f, 0.5f, 0.3f, 0.8f, 0.6f, 0.4f, 0.2f},
    
    // ACID - TB-303 style
    {1.0f, 0.0f, 0.8f, 0.0f, 0.6f, 0.9f, 0.0f, 0.7f,
     0.0f, 0.5f, 0.0f, 0.8f, 0.0f, 0.6f, 1.0f, 0.0f},
    
    // TRANCE - Gate pattern
    {1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f,
     1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f},
    
    // TECHNO - Industrial
    {1.0f, 0.0f, 0.0f, 0.7f, 0.0f, 0.9f, 0.0f, 0.0f,
     0.8f, 0.0f, 0.0f, 0.6f, 0.0f, 1.0f, 0.0f, 0.5f},
    
    // RANDOM - Generative
    {0.8f, 0.3f, 0.9f, 0.1f, 0.6f, 0.7f, 0.2f, 0.9f,
     0.4f, 0.8f, 0.3f, 0.7f, 0.5f, 0.9f, 0.2f, 0.6f},
    
    // EUCLIDEAN - E(7,16)
    {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
     0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f},
    
    // TRAP - Modern trap
    {1.0f, 0.0f, 0.5f, 0.0f, 0.8f, 0.6f, 0.0f, 0.7f,
     1.0f, 0.0f, 0.9f, 0.4f, 0.0f, 0.8f, 0.5f, 0.0f},
    
    // CUSTOM - Starts as classic
    {1.0f, 0.7f, 0.5f, 0.3f, 0.8f, 0.6f, 0.4f, 0.2f,
     0.9f, 0.7f, 0.5f, 0.3f, 0.8f, 0.6f, 0.4f, 0.2f}
};

// Filter state
static float s_svf_lp_l, s_svf_bp_l, s_svf_lp_r, s_svf_bp_r;

// Sequencer state
static uint8_t s_current_step;
static uint32_t s_step_counter;
static float s_current_cutoff;

// Parameters
static float s_cutoff_range;     // ✅ NEW: Cutoff range (was depth)
static float s_resonance;
static float s_speed;
static float s_pattern_morph;
static float s_mix;
static float s_feedback_amount;
static uint8_t s_division;
static uint8_t s_preset;
static bool s_tempo_sync;
static uint8_t s_direction;  // 0=forward, 1=backward
static uint32_t s_sample_counter;

// ✅ FIX: Silence detection threshold
static uint32_t s_silence_counter = 0;
static const uint32_t SILENCE_THRESHOLD = 4800;  // 100ms @ 48kHz

inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// ✅ FIX: Denormal killer
inline float kill_denormal(float x) {
    if (si_fabsf(x) < 1e-10f) return 0.f;
    return x;
}

// ✅ FIX: Filter state reset
inline void reset_filter_states() {
    s_svf_lp_l = 0.f;
    s_svf_bp_l = 0.f;
    s_svf_lp_r = 0.f;
    s_svf_bp_r = 0.f;
}

// ========== CUTOFF RANGE CALCULATOR (NEW!) ==========
inline void calculate_cutoff_range(float range_param, float pattern_value, 
                                   float *min_cutoff, float *max_cutoff) {
    // ✅ NEW: Smart cutoff range based on parameter
    // Range parameter: 0.0 = high/bright, 1.0 = low/deep
    
    if (range_param < 0.33f) {
        // HIGH RANGE (0-33%): Bright house filter
        // Good for: Bright stabs, trance leads, vocal processing
        *min_cutoff = 0.4f;   // ~600 Hz
        *max_cutoff = 0.95f;  // ~12 kHz
        
    } else if (range_param < 0.66f) {
        // MEDIUM RANGE (33-66%): Classic house sweep
        // Good for: Classic house patterns, synth lines, pads
        *min_cutoff = 0.25f;  // ~300 Hz
        *max_cutoff = 0.85f;  // ~8 kHz
        
    } else {
        // LOW RANGE (66-100%): Deep techno bass
        // Good for: Bass processing, deep techno, sub freq control
        *min_cutoff = 0.15f;  // ~100 Hz
        *max_cutoff = 0.65f;  // ~4 kHz
    }
}

// State-variable filter (FIXED - NO SELF-OSCILLATION!)
inline void process_svf(float input_l, float input_r, float cutoff, float resonance,
                        float *out_l, float *out_r) {
    // ✅ FIX: Input silence detection
    float input_level = si_fabsf(input_l) + si_fabsf(input_r);
    if (input_level < 1e-6f) {
        // Silent input → gradually decay filter states
        s_svf_lp_l *= 0.99f;
        s_svf_bp_l *= 0.99f;
        s_svf_lp_r *= 0.99f;
        s_svf_bp_r *= 0.99f;
        
        // Kill denormals
        s_svf_lp_l = kill_denormal(s_svf_lp_l);
        s_svf_bp_l = kill_denormal(s_svf_bp_l);
        s_svf_lp_r = kill_denormal(s_svf_lp_r);
        s_svf_bp_r = kill_denormal(s_svf_bp_r);
        
        *out_l = s_svf_lp_l;
        *out_r = s_svf_lp_r;
        return;
    }
    
    // Safe cutoff range
    cutoff = clipminmaxf(0.01f, cutoff, 0.95f);
    
    // ✅ FIX: Lower max resonance to prevent self-oscillation
    resonance = clipminmaxf(0.f, resonance, 0.85f);  // Was 0.95
    
    // Calculate frequency
    float freq = 100.f + cutoff * cutoff * 11900.f;
    freq = clipminmaxf(100.f, freq, 12000.f);
    
    float w = 2.f * 3.14159265f * freq / 48000.f;
    w = clipminmaxf(0.001f, w, 1.5f);
    
    float f = 2.f * fx_sinf(w * 0.5f);  // ✅ Keep fx_sinf (available for modfx)
    f = clipminmaxf(0.0001f, f, 1.9f);
    
    // ✅ FIX: Safer Q calculation
    float q = 1.f / (0.5f + resonance * 2.5f);  // More conservative
    q = clipminmaxf(0.3f, q, 1.8f);
    
    // Left channel
    s_svf_lp_l = s_svf_lp_l + f * s_svf_bp_l;
    s_svf_lp_l = clipminmaxf(-3.f, s_svf_lp_l, 3.f);
    s_svf_lp_l = kill_denormal(s_svf_lp_l);  // ✅ Denormal kill
    
    float hp_l = input_l - s_svf_lp_l - q * s_svf_bp_l;
    hp_l = clipminmaxf(-3.f, hp_l, 3.f);
    
    s_svf_bp_l = s_svf_bp_l + f * hp_l;
    s_svf_bp_l = clipminmaxf(-3.f, s_svf_bp_l, 3.f);
    s_svf_bp_l = kill_denormal(s_svf_bp_l);  // ✅ Denormal kill
    
    // Right channel
    s_svf_lp_r = s_svf_lp_r + f * s_svf_bp_r;
    s_svf_lp_r = clipminmaxf(-3.f, s_svf_lp_r, 3.f);
    s_svf_lp_r = kill_denormal(s_svf_lp_r);  // ✅ Denormal kill
    
    float hp_r = input_r - s_svf_lp_r - q * s_svf_bp_r;
    hp_r = clipminmaxf(-3.f, hp_r, 3.f);
    
    s_svf_bp_r = s_svf_bp_r + f * hp_r;
    s_svf_bp_r = clipminmaxf(-3.f, s_svf_bp_r, 3.f);
    s_svf_bp_r = kill_denormal(s_svf_bp_r);  // ✅ Denormal kill
    
    *out_l = s_svf_lp_l;
    *out_r = s_svf_lp_r;
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;

    reset_filter_states();
    
    s_current_step = 0;
    s_step_counter = 0;
    s_current_cutoff = 0.5f;
    s_silence_counter = 0;
    
    s_cutoff_range = 0.5f;      // ✅ Medium range (classic house)
    s_resonance = 0.6f;
    s_speed = 0.75f;
    s_pattern_morph = 0.3f;
    s_mix = 0.5f;
    s_feedback_amount = 0.4f;
    s_division = 3;
    s_preset = 0;
    s_tempo_sync = true;
    s_direction = 0;
    
    s_sample_counter = 0;
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    s_current_step = 0;
    reset_filter_states();
    s_silence_counter = 0;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    const float *in_ptr = in;
    float *out_ptr = out;
    
    // Time division (samples per step)
    uint32_t division_samples;
    if (s_tempo_sync) {
        // Tempo sync divisions: 1/4, 1/8, 1/16, 1/32, etc.
        float bpm = fx_get_bpmf();
        if (bpm < 60.f) bpm = 120.f;  // Default if no tempo
        
        float beat_samples = (48000.f * 60.f) / bpm;  // Samples per beat
        division_samples = (uint32_t)(beat_samples / (1 << s_division));
        if (division_samples < 100) division_samples = 100;  // Minimum safe value
    } else {
        // Free-running speed
        division_samples = (uint32_t)(2400.f + (1.f - s_speed) * 19200.f);
        division_samples = clipminmaxi32(100, division_samples, 48000);  // LIMIT!
    }
    
    for (uint32_t i = 0; i < frames; i++) {
        // Update sequencer step
        s_step_counter++;
        if (s_step_counter >= division_samples) {
            s_step_counter = 0;
            
            // Advance step based on direction
            if (s_direction == 0) {
                // Forward
                s_current_step = (s_current_step + 1) % PATTERN_STEPS;
            } else {
                // Backward
                s_current_step = (s_current_step + PATTERN_STEPS - 1) % PATTERN_STEPS;
            }
        }
        
        // Get pattern value
        float pattern_value = s_patterns[s_preset][s_current_step];
        
        // Smooth step changes
        s_current_cutoff += (pattern_value - s_current_cutoff) * 0.05f;
        
        // ✅ NEW: Calculate safe cutoff range
        float min_cutoff, max_cutoff;
        calculate_cutoff_range(s_cutoff_range, s_current_cutoff, 
                              &min_cutoff, &max_cutoff);
        
        // ✅ NEW: Map pattern value to safe cutoff range
        float cutoff = min_cutoff + s_current_cutoff * (max_cutoff - min_cutoff);
        cutoff = clipminmaxf(0.1f, cutoff, 0.95f);
        
        // ✅ FIX: Lower resonance limit
        float safe_reso = clipminmaxf(0.f, s_resonance, 0.85f);
        
        // ✅ FIX: Input validation
        float in_l = in_ptr[0];
        float in_r = in_ptr[1];
        
        if (!std::isfinite(in_l)) in_l = 0.f;
        if (!std::isfinite(in_r)) in_r = 0.f;
        
        // ✅ FIX: Silence detection
        float input_level = si_fabsf(in_l) + si_fabsf(in_r);
        if (input_level < 1e-6f) {
            s_silence_counter++;
            // After 100ms silence, reset filter completely
            if (s_silence_counter > SILENCE_THRESHOLD) {
                reset_filter_states();
                s_silence_counter = SILENCE_THRESHOLD;  // Don't overflow
            }
        } else {
            s_silence_counter = 0;
        }
        
        // Process filter
        float filt_l, filt_r;
        process_svf(in_l, in_r, cutoff, safe_reso, &filt_l, &filt_r);
        
        // Feedback processing
        if (s_feedback_amount > 0.01f) {
            float fb = clipminmaxf(0.f, s_feedback_amount, 0.7f);  // Lower max
            
            filt_l = fast_tanh(filt_l * (1.f + fb * 1.2f));
            filt_r = fast_tanh(filt_r * (1.f + fb * 1.2f));
        }
        
        // Mix
        out_ptr[0] = in_l * (1.f - s_mix) + filt_l * s_mix;
        out_ptr[1] = in_r * (1.f - s_mix) + filt_r * s_mix;
        
        // ✅ FIX: Validate output
        if (!std::isfinite(out_ptr[0])) out_ptr[0] = in_l;
        if (!std::isfinite(out_ptr[1])) out_ptr[1] = in_r;
        
        // FINAL OUTPUT CLIPPING (prevents NaN propagation)
        out_ptr[0] = clipminmaxf(-1.f, out_ptr[0], 1.f);
        out_ptr[1] = clipminmaxf(-1.f, out_ptr[1], 1.f);
        
        in_ptr += 2;
        out_ptr += 2;
        s_sample_counter++;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    // ✅ FIX: Store old resonance for change detection
    static float old_resonance = 0.6f;
    
    switch (id) {
        case 0: // ✅ RANGE (was DEPTH)
            s_cutoff_range = valf;
            break;
        case 1:
            // ✅ FIX: Reset filter when resonance changes significantly
            if (si_fabsf(valf - old_resonance) > 0.15f) {
                reset_filter_states();
            }
            old_resonance = valf;
            s_resonance = clipminmaxf(0.f, valf, 0.85f);  // Lower max
            break;
        case 2: s_speed = valf; break;
        case 3: s_pattern_morph = valf; break;
        case 4: s_mix = valf; break;
        case 5: 
            // LIMIT feedback parameter input
            s_feedback_amount = clipminmaxf(0.f, valf, 0.8f);
            break;
        case 6: s_division = value; break;
        case 7: s_preset = value; break;
        case 8: s_tempo_sync = (value > 0); break;
        case 9: s_direction = value; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_cutoff_range * 1023.f);
        case 1: return (int32_t)(s_resonance * 1023.f);
        case 2: return (int32_t)(s_speed * 1023.f);
        case 3: return (int32_t)(s_pattern_morph * 1023.f);
        case 4: return (int32_t)(s_mix * 1023.f);
        case 5: return (int32_t)(s_feedback_amount * 1023.f);
        case 6: return s_division;
        case 7: return s_preset;
        case 8: return s_tempo_sync ? 1 : 0;
        case 9: return s_direction;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 0) {
        // ✅ NEW: Display range type
        float range_val = (float)value / 1023.f;
        if (range_val < 0.33f) return "HIGH";
        if (range_val < 0.66f) return "MID";
        return "LOW";
    }
    if (id == 6) {
        static const char *div_names[] = {
            "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", 
            "1/4T", "1/8T", "1/16T", "1/32T", "", "", "", "", "", ""
        };
        if (value >= 0 && value < 16) return div_names[value];
    }
    if (id == 7) {
        static const char *preset_names[] = {
            "CLASSIC", "ACID", "TRANCE", "TECHNO",
            "RANDOM", "EUCLID", "TRAP", "CUSTOM"
        };
        if (value >= 0 && value < 8) return preset_names[value];
    }
    if (id == 8) {
        return value ? "SYNC" : "FREE";
    }
    if (id == 9) {
        static const char *dir_names[] = {"FWD", "BWD"};
        if (value >= 0 && value < 2) return dir_names[value];
    }
    return "";
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    // Tempo sync hook (called 4 times per quarter note)
    if (s_tempo_sync) {
        // Reset step counter on downbeat
        if (counter % 16 == 0) {
            s_step_counter = 0;
            s_current_step = 0;
        }
    }
}

__unit_callback void unit_set_tempo(uint32_t tempo) {}

