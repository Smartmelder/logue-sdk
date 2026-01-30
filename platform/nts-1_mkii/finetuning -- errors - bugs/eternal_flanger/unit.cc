/*
    ETERNAL FLANGER - Barber-Pole Flanger Effect
    
    ARCHITECTURE:
    - 4× cascaded delay lines for barber-pole illusion
    - Crossfading LFO system (smooth transitions)
    - 3 direction modes: UP / DOWN / BOTH
    - Feedback network with tone control
    - Stereo widening with phase offset
    
    ALGORITHM:
    - Each delay stage has independent LFO phase
    - Stages crossfade to create endless sweep illusion
    - No audible resets or jumps
*/

#include "unit_modfx.h"
#include "fx_api.h"
#include "utils/float_math.h"

// ========== DIRECTION MODES ==========

enum Direction {
    DIR_UP = 0,      // Endless rising sweep
    DIR_DOWN = 1,    // Endless falling sweep
    DIR_BOTH = 2     // Cycling up/down (classic)
};

// ========== DELAY BUFFER ==========

#define MAX_DELAY_SAMPLES 2400  // 50ms @ 48kHz
#define NUM_STAGES 4

static float *s_delay_buffer_l = nullptr;
static float *s_delay_buffer_r = nullptr;
static uint32_t s_write_pos = 0;

// ========== BARBER-POLE STAGES ==========

struct FlangerStage {
    float lfo_phase;
    float crossfade_level;
    float feedback_state_l;
    float feedback_state_r;
};

static FlangerStage s_stages[NUM_STAGES];

// ========== TONE FILTER ==========

static float s_tone_z1_l = 0.f;
static float s_tone_z1_r = 0.f;

// ========== PARAMETERS ==========

static Direction s_direction = DIR_BOTH;
static float s_rate = 0.3f;
static float s_depth = 0.5f;
static float s_feedback = 0.4f;
static float s_mix = 0.5f;
static float s_stereo = 0.6f;
static float s_tone = 0.5f;
static float s_smooth = 0.7f;
static uint8_t s_active_stages = 4;
static float s_resonate = 0.3f;

// ========== BARBER-POLE LFO ==========

inline float barber_pole_lfo(float phase, Direction dir) {
    // Normalize phase [0, 1]
    while (phase >= 1.f) phase -= 1.f;
    while (phase < 0.f) phase += 1.f;
    
    switch (dir) {
        case DIR_UP:
            // Continuous rise illusion
            return phase;
            
        case DIR_DOWN:
            // Continuous fall illusion
            return 1.f - phase;
            
        case DIR_BOTH:
        default:
            // Classic triangle LFO
            return (phase < 0.5f) ? (phase * 2.f) : (2.f - phase * 2.f);
    }
}

// ========== CROSSFADE CALCULATOR ==========

inline void calculate_crossfade_levels() {
    // Calculate crossfade between stages for smooth barber-pole
    for (uint8_t i = 0; i < NUM_STAGES; i++) {
        float phase = s_stages[i].lfo_phase;
        
        // Smooth crossfade window
        float window = s_smooth;
        
        if (s_direction == DIR_UP || s_direction == DIR_DOWN) {
            // Barber-pole: fade in/out at boundaries
            if (phase < window) {
                // Fade in
                s_stages[i].crossfade_level = phase / window;
            } else if (phase > (1.f - window)) {
                // Fade out
                s_stages[i].crossfade_level = (1.f - phase) / window;
            } else {
                // Full level
                s_stages[i].crossfade_level = 1.f;
            }
        } else {
            // Classic mode: always full level
            s_stages[i].crossfade_level = 1.f;
        }
        
        // Smooth crossfade
        s_stages[i].crossfade_level = clipminmaxf(0.f, s_stages[i].crossfade_level, 1.f);
    }
}

// ========== DELAY READ WITH INTERPOLATION ==========

inline float delay_read(float *buffer, float delay_samples) {
    delay_samples = clipminmaxf(1.f, delay_samples, (float)(MAX_DELAY_SAMPLES - 2));
    
    float read_pos_f = (float)s_write_pos - delay_samples;
    
    while (read_pos_f < 0.f) read_pos_f += (float)MAX_DELAY_SAMPLES;
    while (read_pos_f >= (float)MAX_DELAY_SAMPLES) read_pos_f -= (float)MAX_DELAY_SAMPLES;
    
    uint32_t read_pos_0 = (uint32_t)read_pos_f;
    uint32_t read_pos_1 = (read_pos_0 + 1) % MAX_DELAY_SAMPLES;
    
    float frac = read_pos_f - (float)read_pos_0;
    
    return buffer[read_pos_0] * (1.f - frac) + buffer[read_pos_1] * frac;
}

// ========== TONE FILTER ==========

inline void apply_tone(float *l, float *r) {
    float coeff = 0.2f + s_tone * 0.6f;
    
    s_tone_z1_l += coeff * (*l - s_tone_z1_l);
    s_tone_z1_r += coeff * (*r - s_tone_z1_r);
    
    // Denormal kill
    if (si_fabsf(s_tone_z1_l) < 1e-15f) s_tone_z1_l = 0.f;
    if (si_fabsf(s_tone_z1_r) < 1e-15f) s_tone_z1_r = 0.f;
    
    // Blend
    *l = s_tone_z1_l * (1.f - s_tone * 0.3f) + *l * (0.7f + s_tone * 0.3f);
    *r = s_tone_z1_r * (1.f - s_tone * 0.3f) + *r * (0.7f + s_tone * 0.3f);
}

// ========== STEREO WIDENER ==========

inline void apply_stereo(float *l, float *r) {
    float mid = (*l + *r) * 0.5f;
    float side = (*l - *r) * 0.5f;
    
    // Width: 0-200%
    float width = s_stereo * 2.f;
    side *= width;
    
    *l = mid + side;
    *r = mid - side;
}

// ========== MAIN FLANGER PROCESSOR ==========

inline void process_eternal_flanger(float in_l, float in_r, float *out_l, float *out_r) {
    // Input validation
    if (!std::isfinite(in_l)) in_l = 0.f;
    if (!std::isfinite(in_r)) in_r = 0.f;
    
    // Write input to delay buffer
    if (s_delay_buffer_l && s_delay_buffer_r) {
        s_delay_buffer_l[s_write_pos] = in_l;
        s_delay_buffer_r[s_write_pos] = in_r;
    }
    
    // Calculate crossfade levels
    calculate_crossfade_levels();
    
    float wet_l = 0.f;
    float wet_r = 0.f;
    float total_crossfade = 0.f;
    
    // Process each active stage
    for (uint8_t i = 0; i < s_active_stages; i++) {
        FlangerStage *stage = &s_stages[i];
        
        // Get LFO value for this stage
        float lfo = barber_pole_lfo(stage->lfo_phase, s_direction);
        
        // Calculate delay time (0.5ms - 15ms base range)
        float base_delay = 1.f + s_depth * 14.f;  // 1-15ms
        float mod_delay = lfo * s_depth * 10.f;   // Modulation amount
        
        float delay_ms = base_delay + mod_delay;
        delay_ms = clipminmaxf(0.5f, delay_ms, 20.f);
        
        float delay_samples = delay_ms * 48.f;  // ms to samples
        
        // Read from delay buffer
        float delayed_l = delay_read(s_delay_buffer_l, delay_samples);
        float delayed_r = delay_read(s_delay_buffer_r, delay_samples);
        
        // Validate delayed samples
        if (!std::isfinite(delayed_l)) delayed_l = 0.f;
        if (!std::isfinite(delayed_r)) delayed_r = 0.f;
        
        // Apply feedback
        if (s_feedback > 0.01f) {
            stage->feedback_state_l = delayed_l + stage->feedback_state_l * s_feedback * 0.6f;
            stage->feedback_state_r = delayed_r + stage->feedback_state_r * s_feedback * 0.6f;
            
            // Clip feedback
            stage->feedback_state_l = clipminmaxf(-2.f, stage->feedback_state_l, 2.f);
            stage->feedback_state_r = clipminmaxf(-2.f, stage->feedback_state_r, 2.f);
            
            // Denormal kill
            if (si_fabsf(stage->feedback_state_l) < 1e-15f) stage->feedback_state_l = 0.f;
            if (si_fabsf(stage->feedback_state_r) < 1e-15f) stage->feedback_state_r = 0.f;
            
            delayed_l = stage->feedback_state_l;
            delayed_r = stage->feedback_state_r;
        }
        
        // Apply resonance peak emphasis
        if (s_resonate > 0.01f) {
            float resonance_boost = 1.f + s_resonate * lfo * 0.5f;
            delayed_l *= resonance_boost;
            delayed_r *= resonance_boost;
        }
        
        // Apply crossfade level
        float level = stage->crossfade_level;
        
        wet_l += delayed_l * level;
        wet_r += delayed_r * level;
        total_crossfade += level;
        
        // Update LFO phase
        float rate_hz = 0.05f + s_rate * 7.95f;  // 0.05-8 Hz
        
        // Phase offset between stages for barber-pole
        float phase_offset = (float)i / (float)NUM_STAGES;
        
        stage->lfo_phase += rate_hz / 48000.f;
        
        // Wrap phase
        if (stage->lfo_phase >= 1.f) {
            stage->lfo_phase -= 1.f;
        }
    }
    
    // Normalize by total crossfade
    if (total_crossfade > 0.01f) {
        wet_l /= total_crossfade;
        wet_r /= total_crossfade;
    }
    
    // Apply tone filter
    apply_tone(&wet_l, &wet_r);
    
    // Apply stereo width
    apply_stereo(&wet_l, &wet_r);
    
    // Output validation
    if (!std::isfinite(wet_l)) wet_l = 0.f;
    if (!std::isfinite(wet_r)) wet_r = 0.f;
    
    // Dry/wet mix
    *out_l = in_l * (1.f - s_mix) + wet_l * s_mix;
    *out_r = in_r * (1.f - s_mix) + wet_r * s_mix;
}

// ========== UNIT CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;
    if (!desc->hooks.sdram_alloc) return k_unit_err_memory;
    
    // Allocate SDRAM buffer (single allocation)
    size_t total_size = MAX_DELAY_SAMPLES * sizeof(float) * 2;
    uint8_t *buffer_base = static_cast<uint8_t *>(desc->hooks.sdram_alloc(total_size));
    
    if (!buffer_base) return k_unit_err_memory;
    
    s_delay_buffer_l = reinterpret_cast<float *>(buffer_base);
    s_delay_buffer_r = reinterpret_cast<float *>(buffer_base + MAX_DELAY_SAMPLES * sizeof(float));
    
    // Clear buffers
    for (uint32_t i = 0; i < MAX_DELAY_SAMPLES; i++) {
        s_delay_buffer_l[i] = 0.f;
        s_delay_buffer_r[i] = 0.f;
    }
    
    s_write_pos = 0;
    
    // Initialize stages with phase offset
    for (uint8_t i = 0; i < NUM_STAGES; i++) {
        s_stages[i].lfo_phase = (float)i / (float)NUM_STAGES;
        s_stages[i].crossfade_level = 1.f;
        s_stages[i].feedback_state_l = 0.f;
        s_stages[i].feedback_state_r = 0.f;
    }
    
    s_tone_z1_l = 0.f;
    s_tone_z1_r = 0.f;
    
    s_direction = DIR_BOTH;
    s_rate = 0.3f;
    s_depth = 0.5f;
    s_feedback = 0.4f;
    s_mix = 0.5f;
    s_stereo = 0.6f;
    s_tone = 0.5f;
    s_smooth = 0.7f;
    s_active_stages = 4;
    s_resonate = 0.3f;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    if (s_delay_buffer_l) {
        for (uint32_t i = 0; i < MAX_DELAY_SAMPLES; i++) {
            s_delay_buffer_l[i] = 0.f;
        }
    }
    if (s_delay_buffer_r) {
        for (uint32_t i = 0; i < MAX_DELAY_SAMPLES; i++) {
            s_delay_buffer_r[i] = 0.f;
        }
    }
    
    s_write_pos = 0;
    
    for (uint8_t i = 0; i < NUM_STAGES; i++) {
        s_stages[i].feedback_state_l = 0.f;
        s_stages[i].feedback_state_r = 0.f;
    }
    
    s_tone_z1_l = 0.f;
    s_tone_z1_r = 0.f;
}

__unit_callback void unit_resume() {}

__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    const float *in_ptr = in;
    float *out_ptr = out;
    
    for (uint32_t f = 0; f < frames; f++) {
        float out_l, out_r;
        process_eternal_flanger(in_ptr[0], in_ptr[1], &out_l, &out_r);
        
        // Final limiting
        out_ptr[0] = clipminmaxf(-1.f, out_l, 1.f);
        out_ptr[1] = clipminmaxf(-1.f, out_r, 1.f);
        
        s_write_pos = (s_write_pos + 1) % MAX_DELAY_SAMPLES;
        
        in_ptr += 2;
        out_ptr += 2;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: // Direction
            s_direction = (Direction)value;
            break;
        case 1: // Rate
            s_rate = valf;
            break;
        case 2: // Depth
            s_depth = valf;
            break;
        case 3: // Feedback
            s_feedback = valf;
            break;
        case 4: // Mix
            s_mix = valf;
            break;
        case 5: // Stereo
            s_stereo = valf;
            break;
        case 6: // Tone
            s_tone = valf;
            break;
        case 7: // Smooth
            s_smooth = valf * 0.5f;  // 0-50% window
            break;
        case 8: // Stages
            s_active_stages = (uint8_t)value;
            if (s_active_stages < 2) s_active_stages = 2;
            if (s_active_stages > 4) s_active_stages = 4;
            break;
        case 9: // Resonate
            s_resonate = valf;
            break;
        default:
            break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)s_direction;
        case 1: return (int32_t)(s_rate * 1023.f);
        case 2: return (int32_t)(s_depth * 1023.f);
        case 3: return (int32_t)(s_feedback * 1023.f);
        case 4: return (int32_t)(s_mix * 1023.f);
        case 5: return (int32_t)(s_stereo * 1023.f);
        case 6: return (int32_t)(s_tone * 1023.f);
        case 7: return (int32_t)((s_smooth / 0.5f) * 1023.f);
        case 8: return s_active_stages;
        case 9: return (int32_t)(s_resonate * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 0) {
        switch ((Direction)value) {
            case DIR_UP: return "UP";
            case DIR_DOWN: return "DOWN";
            case DIR_BOTH: return "BOTH";
        }
    }
    if (id == 8) {
        static char buf[4];
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
