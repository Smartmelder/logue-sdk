/*
    KUTCHORUS - Ultimate Multi-Mode Chorus
    
    Advanced chorus for techno & house
*/

#include "unit_modfx.h"
#include "fx_api.h"  // ✅ For fx_pow2f() and fx_sinf()
#include "utils/float_math.h"  // ✅ For si_fabsf(), si_floorf()
#include "utils/int_math.h"

// ========== FAST TANH (for effects) ==========

inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// ========== IS FINITE CHECK ==========

#define is_finite(x) ((x) == (x) && (x) < 1e10f && (x) > -1e10f)

// ========== CHORUS TYPES ==========

enum ChorusType {
    TYPE_SOFT = 0,      // 0-24%: Subtle widening
    TYPE_CLASSIC,       // 25-49%: Juno/'80s style
    TYPE_WIDE,          // 50-74%: Big chorus
    TYPE_DIRTY          // 75-100%: Aggressive techno
};

const char* type_names[4] = {
    "SOFT", "CLASSIC", "WIDE", "DIRTY"
};

// ========== DELAY BUFFER ==========

#define MAX_DELAY_SAMPLES 2400  // 50ms @ 48kHz
#define NUM_VOICES 4

// ✅ SDRAM buffers (allocated in unit_init)
static float *s_delay_buffer_l = nullptr;
static float *s_delay_buffer_r = nullptr;
static uint32_t s_delay_write_pos = 0;

// ========== CHORUS VOICE ==========

struct ChorusVoice {
    float lfo_phase;
    float base_delay_ms;
    float phase_offset;
    float pan;          // L/R position
    float level;
    float feedback_state_l;
    float feedback_state_r;
};

static ChorusVoice s_voices[NUM_VOICES];

// ========== TONE FILTER ==========

static float s_tone_z1_l = 0.f;
static float s_tone_z1_r = 0.f;

// ========== BASS CUT FILTER ==========

static float s_bass_hp_z1_l = 0.f;
static float s_bass_hp_z1_r = 0.f;

// ========== PARAMETERS ==========

static uint8_t s_chorus_type = TYPE_CLASSIC;
static float s_rate = 0.3f;
static float s_depth = 0.5f;
static float s_mix = 0.5f;
static float s_width = 0.5f;
static float s_tone = 0.5f;
static float s_motion = 0.2f;
static float s_bass_cut = 0.3f;
static uint8_t s_voice_count = 3;
static float s_feedback = 0.1f;

// Random state for motion
static uint32_t s_rand_state = 12345;

// ========== RANDOM GENERATOR ==========

inline float random_float() {
    s_rand_state ^= s_rand_state << 13;
    s_rand_state ^= s_rand_state >> 17;
    s_rand_state ^= s_rand_state << 5;
    return (float)(s_rand_state % 10000) / 10000.f;
}

// ========== CHORUS TYPE CONFIGURATION (FIXED!) ==========

inline void configure_chorus_type() {
    // ✅ FIX: Reset ALL voice state when changing type!
    
    switch (s_chorus_type) {
        case TYPE_SOFT:
            // 2 voices, subtle
            s_voices[0].base_delay_ms = 10.f;
            s_voices[0].phase_offset = 0.f;
            s_voices[0].pan = -0.3f;
            s_voices[0].level = 0.7f;
            s_voices[0].lfo_phase = 0.f;              // ✅ RESET!
            s_voices[0].feedback_state_l = 0.f;       // ✅ RESET!
            s_voices[0].feedback_state_r = 0.f;       // ✅ RESET!
            
            s_voices[1].base_delay_ms = 18.f;
            s_voices[1].phase_offset = 0.5f;
            s_voices[1].pan = 0.3f;
            s_voices[1].level = 0.7f;
            s_voices[1].lfo_phase = 0.5f;             // ✅ RESET! (with offset)
            s_voices[1].feedback_state_l = 0.f;       // ✅ RESET!
            s_voices[1].feedback_state_r = 0.f;       // ✅ RESET!
            
            s_voices[2].level = 0.f;  // Inactive
            s_voices[2].lfo_phase = 0.f;              // ✅ RESET!
            s_voices[2].feedback_state_l = 0.f;       // ✅ RESET!
            s_voices[2].feedback_state_r = 0.f;       // ✅ RESET!
            
            s_voices[3].level = 0.f;
            s_voices[3].lfo_phase = 0.f;              // ✅ RESET!
            s_voices[3].feedback_state_l = 0.f;       // ✅ RESET!
            s_voices[3].feedback_state_r = 0.f;       // ✅ RESET!
            break;
            
        case TYPE_CLASSIC:
            // 3 voices, Juno-style
            s_voices[0].base_delay_ms = 8.f;
            s_voices[0].phase_offset = 0.f;
            s_voices[0].pan = -0.5f;
            s_voices[0].level = 0.6f;
            s_voices[0].lfo_phase = 0.f;              // ✅ RESET!
            s_voices[0].feedback_state_l = 0.f;       // ✅ RESET!
            s_voices[0].feedback_state_r = 0.f;       // ✅ RESET!
            
            s_voices[1].base_delay_ms = 12.f;
            s_voices[1].phase_offset = 0.33f;
            s_voices[1].pan = 0.f;
            s_voices[1].level = 0.6f;
            s_voices[1].lfo_phase = 0.33f;            // ✅ RESET! (with offset)
            s_voices[1].feedback_state_l = 0.f;       // ✅ RESET!
            s_voices[1].feedback_state_r = 0.f;       // ✅ RESET!
            
            s_voices[2].base_delay_ms = 15.f;
            s_voices[2].phase_offset = 0.66f;
            s_voices[2].pan = 0.5f;
            s_voices[2].level = 0.6f;
            s_voices[2].lfo_phase = 0.66f;            // ✅ RESET! (with offset)
            s_voices[2].feedback_state_l = 0.f;       // ✅ RESET!
            s_voices[2].feedback_state_r = 0.f;       // ✅ RESET!
            
            s_voices[3].level = 0.f;
            s_voices[3].lfo_phase = 0.f;              // ✅ RESET!
            s_voices[3].feedback_state_l = 0.f;       // ✅ RESET!
            s_voices[3].feedback_state_r = 0.f;       // ✅ RESET!
            break;
            
        case TYPE_WIDE:
            // 4 voices, wide spread
            s_voices[0].base_delay_ms = 6.f;
            s_voices[0].phase_offset = 0.f;
            s_voices[0].pan = -0.8f;
            s_voices[0].level = 0.5f;
            s_voices[0].lfo_phase = 0.f;              // ✅ RESET!
            s_voices[0].feedback_state_l = 0.f;       // ✅ RESET!
            s_voices[0].feedback_state_r = 0.f;       // ✅ RESET!
            
            s_voices[1].base_delay_ms = 11.f;
            s_voices[1].phase_offset = 0.25f;
            s_voices[1].pan = -0.3f;
            s_voices[1].level = 0.5f;
            s_voices[1].lfo_phase = 0.25f;            // ✅ RESET! (with offset)
            s_voices[1].feedback_state_l = 0.f;       // ✅ RESET!
            s_voices[1].feedback_state_r = 0.f;       // ✅ RESET!
            
            s_voices[2].base_delay_ms = 16.f;
            s_voices[2].phase_offset = 0.5f;
            s_voices[2].pan = 0.3f;
            s_voices[2].level = 0.5f;
            s_voices[2].lfo_phase = 0.5f;             // ✅ RESET! (with offset)
            s_voices[2].feedback_state_l = 0.f;       // ✅ RESET!
            s_voices[2].feedback_state_r = 0.f;       // ✅ RESET!
            
            s_voices[3].base_delay_ms = 22.f;
            s_voices[3].phase_offset = 0.75f;
            s_voices[3].pan = 0.8f;
            s_voices[3].level = 0.5f;
            s_voices[3].lfo_phase = 0.75f;            // ✅ RESET! (with offset)
            s_voices[3].feedback_state_l = 0.f;       // ✅ RESET!
            s_voices[3].feedback_state_r = 0.f;       // ✅ RESET!
            break;
            
        case TYPE_DIRTY:
            // 4 voices, aggressive
            s_voices[0].base_delay_ms = 5.f;
            s_voices[0].phase_offset = 0.f;
            s_voices[0].pan = -0.9f;
            s_voices[0].level = 0.6f;
            s_voices[0].lfo_phase = 0.f;              // ✅ RESET!
            s_voices[0].feedback_state_l = 0.f;       // ✅ RESET!
            s_voices[0].feedback_state_r = 0.f;       // ✅ RESET!
            
            s_voices[1].base_delay_ms = 9.f;
            s_voices[1].phase_offset = 0.3f;
            s_voices[1].pan = -0.4f;
            s_voices[1].level = 0.6f;
            s_voices[1].lfo_phase = 0.3f;             // ✅ RESET! (with offset)
            s_voices[1].feedback_state_l = 0.f;       // ✅ RESET!
            s_voices[1].feedback_state_r = 0.f;       // ✅ RESET!
            
            s_voices[2].base_delay_ms = 14.f;
            s_voices[2].phase_offset = 0.6f;
            s_voices[2].pan = 0.4f;
            s_voices[2].level = 0.6f;
            s_voices[2].lfo_phase = 0.6f;             // ✅ RESET! (with offset)
            s_voices[2].feedback_state_l = 0.f;       // ✅ RESET!
            s_voices[2].feedback_state_r = 0.f;       // ✅ RESET!
            
            s_voices[3].base_delay_ms = 20.f;
            s_voices[3].phase_offset = 0.9f;
            s_voices[3].pan = 0.9f;
            s_voices[3].level = 0.6f;
            s_voices[3].lfo_phase = 0.9f;             // ✅ RESET! (with offset)
            s_voices[3].feedback_state_l = 0.f;       // ✅ RESET!
            s_voices[3].feedback_state_r = 0.f;       // ✅ RESET!
            break;
    }
    
    // ✅ EXTRA: Clear delay buffer write position (smooth transition)
    // Don't reset completely, just ensure clean state
    // s_delay_write_pos stays as is for continuity
}

// ========== SMOOTH TYPE TRANSITION ==========

inline void smooth_type_transition() {
    // When switching types, apply gentle fade to avoid clicks
    // This is called automatically in unit_set_param_value
    
    // ✅ Optional: Clear tone filters for clean switch
    s_tone_z1_l *= 0.5f;
    s_tone_z1_r *= 0.5f;
    s_bass_hp_z1_l *= 0.5f;
    s_bass_hp_z1_r *= 0.5f;
}

// ========== LFO GENERATOR ==========

inline float generate_lfo(float phase) {
    // Sine LFO
    return fx_sinf(phase);
}

// ========== BASS CUT FILTER ==========

inline void process_bass_cut(float *wet_l, float *wet_r) {
    if (s_bass_cut < 0.01f) return;
    
    // High-pass filter on wet signal
    // Cutoff frequency based on s_bass_cut (150-400 Hz)
    float cutoff = 150.f + s_bass_cut * 250.f;
    float w = 2.f * 3.14159265f * cutoff / 48000.f;
    float coeff = 1.f - w;
    coeff = clipminmaxf(0.9f, coeff, 0.999f);
    
    // One-pole HP
    float hp_l = *wet_l - s_bass_hp_z1_l;
    s_bass_hp_z1_l += coeff * (*wet_l - s_bass_hp_z1_l);
    
    float hp_r = *wet_r - s_bass_hp_z1_r;
    s_bass_hp_z1_r += coeff * (*wet_r - s_bass_hp_z1_r);
    
    // Denormal kill
    if (si_fabsf(s_bass_hp_z1_l) < 1e-15f) s_bass_hp_z1_l = 0.f;
    if (si_fabsf(s_bass_hp_z1_r) < 1e-15f) s_bass_hp_z1_r = 0.f;
    
    // Mix HP with original based on bass_cut amount
    *wet_l = hp_l * s_bass_cut + *wet_l * (1.f - s_bass_cut);
    *wet_r = hp_r * s_bass_cut + *wet_r * (1.f - s_bass_cut);
}

// ========== TONE CONTROL ==========

inline void process_tone(float *wet_l, float *wet_r) {
    // Tilt EQ: <50% = darker, >50% = brighter
    float tilt = (s_tone - 0.5f) * 2.f;  // -1 to +1
    
    if (tilt < 0.f) {
        // Darker: low-pass
        float lp_coeff = 0.3f + (1.f + tilt) * 0.4f;
        lp_coeff = clipminmaxf(0.1f, lp_coeff, 0.9f);
        
        s_tone_z1_l += lp_coeff * (*wet_l - s_tone_z1_l);
        s_tone_z1_r += lp_coeff * (*wet_r - s_tone_z1_r);
        
        *wet_l = s_tone_z1_l;
        *wet_r = s_tone_z1_r;
    } else {
        // Brighter: high-shelf
        float hp = *wet_l - s_tone_z1_l;
        s_tone_z1_l += 0.3f * (*wet_l - s_tone_z1_l);
        *wet_l = *wet_l + hp * tilt * 0.5f;
        
        hp = *wet_r - s_tone_z1_r;
        s_tone_z1_r += 0.3f * (*wet_r - s_tone_z1_r);
        *wet_r = *wet_r + hp * tilt * 0.5f;
    }
    
    // Denormal kill
    if (si_fabsf(s_tone_z1_l) < 1e-15f) s_tone_z1_l = 0.f;
    if (si_fabsf(s_tone_z1_r) < 1e-15f) s_tone_z1_r = 0.f;
}

// ========== CHORUS PROCESSOR ==========

inline void process_chorus(float in_l, float in_r, float *out_l, float *out_r) {
    // ✅ FIX: Input validation
    if (!is_finite(in_l)) in_l = 0.f;
    if (!is_finite(in_r)) in_r = 0.f;
    
    float wet_l = 0.f;
    float wet_r = 0.f;
    
    // ✅ FIX: Safe voice count (minimum 1)
    uint8_t active_voices = clipminmaxu32(1, s_voice_count, NUM_VOICES);
    
    // Process each voice
    for (uint8_t v = 0; v < NUM_VOICES; v++) {
        if (s_voices[v].level < 0.01f) continue;
        
        ChorusVoice* voice = &s_voices[v];
        
        // Generate LFO
        float lfo = generate_lfo(voice->lfo_phase);
        
        // Add motion (random drift)
        if (s_motion > 0.01f) {
            float drift = (random_float() - 0.5f) * s_motion * 0.1f;
            lfo += drift;
        }
        
        // Calculate modulated delay time
        float delay_time_ms = voice->base_delay_ms + lfo * s_depth * 5.f;
        delay_time_ms = clipminmaxf(3.f, delay_time_ms, 30.f);
        
        uint32_t delay_samples = (uint32_t)(delay_time_ms * 48.f);  // ms to samples
        delay_samples = clipminmaxu32(1, delay_samples, MAX_DELAY_SAMPLES - 1);
        
        // Read from delay buffer
        uint32_t read_pos = (s_delay_write_pos + MAX_DELAY_SAMPLES - delay_samples) % MAX_DELAY_SAMPLES;
        
        float delayed_l = (s_delay_buffer_l) ? s_delay_buffer_l[read_pos] : 0.f;
        float delayed_r = (s_delay_buffer_r) ? s_delay_buffer_r[read_pos] : 0.f;
        
        // ✅ FIX: Validate delayed samples
        if (!is_finite(delayed_l)) delayed_l = 0.f;
        if (!is_finite(delayed_r)) delayed_r = 0.f;
        
        // Apply feedback
        float fb = clipminmaxf(0.f, s_feedback * 0.5f, 0.5f);
        delayed_l += voice->feedback_state_l * fb;
        delayed_r += voice->feedback_state_r * fb;
        
        // Update feedback state
        voice->feedback_state_l = delayed_l * 0.5f;
        voice->feedback_state_r = delayed_r * 0.5f;
        
        // Apply pan and width
        float pan_l = 0.5f - voice->pan * s_width * 0.5f;
        float pan_r = 0.5f + voice->pan * s_width * 0.5f;
        
        pan_l = clipminmaxf(0.f, pan_l, 1.f);
        pan_r = clipminmaxf(0.f, pan_r, 1.f);
        
        wet_l += delayed_l * pan_l * voice->level;
        wet_r += delayed_r * pan_r * voice->level;
        
        // Advance LFO phase
        float lfo_rate = 0.05f + s_rate * 7.95f;  // 0.05-8 Hz
        voice->lfo_phase += lfo_rate / 48000.f;
        if (voice->lfo_phase >= 1.f) voice->lfo_phase -= 1.f;
    }
    
    // ✅ FIX: Safe normalization
    wet_l /= (float)active_voices;
    wet_r /= (float)active_voices;
    
    // Apply bass cut
    process_bass_cut(&wet_l, &wet_r);
    
    // Apply tone
    process_tone(&wet_l, &wet_r);
    
    // Add dirty character for DIRTY mode
    if (s_chorus_type == TYPE_DIRTY) {
        // ✅ FIX: Use fast_tanh() (custom implementation, NOT fastertanh2f!)
        wet_l = fast_tanh(wet_l * 1.2f) * 0.9f;
        wet_r = fast_tanh(wet_r * 1.2f) * 0.9f;
        
        // Gentle bit crush
        float bits = 12.f;  // 12-bit
        float scale = fx_pow2f(bits);
        wet_l = si_floorf(wet_l * scale) / scale;
        wet_r = si_floorf(wet_r * scale) / scale;
        
        // Add subtle noise
        wet_l += (random_float() - 0.5f) * 0.01f;
        wet_r += (random_float() - 0.5f) * 0.01f;
    }
    
    // Write to delay buffer (with input)
    if (s_delay_buffer_l && s_delay_buffer_r) {
        s_delay_buffer_l[s_delay_write_pos] = in_l;
        s_delay_buffer_r[s_delay_write_pos] = in_r;
    }
    
    // ✅ FIX: Output validation
    if (!is_finite(wet_l)) wet_l = 0.f;
    if (!is_finite(wet_r)) wet_r = 0.f;
    
    // Dry/wet mix
    *out_l = in_l * (1.f - s_mix) + wet_l * s_mix;
    *out_r = in_r * (1.f - s_mix) + wet_r * s_mix;
    
    // ✅ FIX: Final output validation
    if (!is_finite(*out_l)) *out_l = 0.f;
    if (!is_finite(*out_r)) *out_r = 0.f;
}

// ========== UNIT CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;
    if (!desc->hooks.sdram_alloc) return k_unit_err_memory;
    
    // ✅ FIX: Single SDRAM allocation for both buffers (no fragmentation)
    size_t total_size = MAX_DELAY_SAMPLES * sizeof(float) * 2;
    uint8_t *buffer_base = static_cast<uint8_t *>(desc->hooks.sdram_alloc(total_size));
    
    if (!buffer_base) return k_unit_err_memory;
    
    s_delay_buffer_l = reinterpret_cast<float *>(buffer_base);
    s_delay_buffer_r = reinterpret_cast<float *>(buffer_base + MAX_DELAY_SAMPLES * sizeof(float));
    
    // Clear delay buffers
    for (uint32_t i = 0; i < MAX_DELAY_SAMPLES; i++) {
        s_delay_buffer_l[i] = 0.f;
        s_delay_buffer_r[i] = 0.f;
    }
    
    s_delay_write_pos = 0;
    
    // Init voices
    for (uint8_t v = 0; v < NUM_VOICES; v++) {
        s_voices[v].lfo_phase = (float)v * 0.25f;  // Phase spread
        s_voices[v].feedback_state_l = 0.f;
        s_voices[v].feedback_state_r = 0.f;
    }
    
    // Clear filters
    s_tone_z1_l = 0.f;
    s_tone_z1_r = 0.f;
    s_bass_hp_z1_l = 0.f;
    s_bass_hp_z1_r = 0.f;
    
    // Init parameters
    s_chorus_type = TYPE_CLASSIC;
    s_rate = 0.3f;
    s_depth = 0.5f;
    s_mix = 0.5f;
    s_width = 0.5f;
    s_tone = 0.5f;
    s_motion = 0.2f;
    s_bass_cut = 0.3f;
    s_voice_count = 3;
    s_feedback = 0.1f;
    
    // Configure chorus type
    configure_chorus_type();
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    if (s_delay_buffer_l && s_delay_buffer_r) {
        for (uint32_t i = 0; i < MAX_DELAY_SAMPLES; i++) {
            s_delay_buffer_l[i] = 0.f;
            s_delay_buffer_r[i] = 0.f;
        }
    }
    
    s_delay_write_pos = 0;
    
    s_tone_z1_l = 0.f;
    s_tone_z1_r = 0.f;
    s_bass_hp_z1_l = 0.f;
    s_bass_hp_z1_r = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    const float *in_ptr = in;
    float *out_ptr = out;
    
    for (uint32_t f = 0; f < frames; f++) {
        float out_l, out_r;
        process_chorus(in_ptr[0], in_ptr[1], &out_l, &out_r);
        
        // Output limiting
        out_ptr[0] = clipminmaxf(-1.f, out_l, 1.f);
        out_ptr[1] = clipminmaxf(-1.f, out_r, 1.f);
        
        // Advance write position
        s_delay_write_pos = (s_delay_write_pos + 1) % MAX_DELAY_SAMPLES;
        
        in_ptr += 2;
        out_ptr += 2;
    }
}

// ========== PARAMETER HANDLING ==========

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: // Type
        {
            float type_val = valf;
            
            // ✅ FIX: Store old type for change detection (static to persist)
            static uint8_t old_type = TYPE_CLASSIC;
            
            if (type_val < 0.25f) {
                s_chorus_type = TYPE_SOFT;
            } else if (type_val < 0.5f) {
                s_chorus_type = TYPE_CLASSIC;
            } else if (type_val < 0.75f) {
                s_chorus_type = TYPE_WIDE;
            } else {
                s_chorus_type = TYPE_DIRTY;
            }
            
            // ✅ FIX: Only reconfigure if type actually changed
            if (s_chorus_type != old_type) {
                configure_chorus_type();
                smooth_type_transition();
                old_type = s_chorus_type;
            }
            break;
        }
        
        case 1: // Rate
            s_rate = valf;
            break;
            
        case 2: // Depth
            s_depth = valf;
            break;
            
        case 3: // Mix
            s_mix = valf;
            break;
            
        case 4: // Width
            s_width = valf;
            break;
            
        case 5: // Tone
            s_tone = valf;
            break;
            
        case 6: // Motion
            s_motion = valf;
            break;
            
        case 7: // Bass Cut
            s_bass_cut = valf;
            break;
            
        case 8: // Voice Count
            s_voice_count = (uint8_t)value;
            s_voice_count = clipminmaxu32(1, s_voice_count, NUM_VOICES);
            break;
            
        case 9: // Feedback
            s_feedback = valf;
            break;
            
        default:
            break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: // Type
        {
            float type_val = 0.f;
            switch (s_chorus_type) {
                case TYPE_SOFT: type_val = 0.125f; break;
                case TYPE_CLASSIC: type_val = 0.375f; break;
                case TYPE_WIDE: type_val = 0.625f; break;
                case TYPE_DIRTY: type_val = 0.875f; break;
            }
            return (int32_t)(type_val * 1023.f);
        }
        case 1: return (int32_t)(s_rate * 1023.f);
        case 2: return (int32_t)(s_depth * 1023.f);
        case 3: return (int32_t)(s_mix * 1023.f);
        case 4: return (int32_t)(s_width * 1023.f);
        case 5: return (int32_t)(s_tone * 1023.f);
        case 6: return (int32_t)(s_motion * 1023.f);
        case 7: return (int32_t)(s_bass_cut * 1023.f);
        case 8: return s_voice_count;
        case 9: return (int32_t)(s_feedback * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 0) {
        return type_names[s_chorus_type];
    }
    
    if (id == 8) {
        static char buf[4];
        // Convert integer value (2-4) to string
        buf[0] = '0' + (char)value;
        buf[1] = '\0';
        return buf;
    }
    
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    (void)tempo;
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    (void)counter;
}

