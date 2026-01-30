/*
    SELF-RANDOMIZING AUDIO REPEATER
    
    ═══════════════════════════════════════════════════════════════
    ARCHITECTURE - THE GENERATIVE BRAIN
    ═══════════════════════════════════════════════════════════════
    
    === GRAIN SYSTEM ===
    
    32 concurrent grains:
    - Each grain: 2048 sample buffer (42ms @ 48kHz)
    - Polyphonic playback (all grains simultaneous)
    - Independent parameters per grain:
      * Position in buffer
      * Playback speed (pitch)
      * Volume envelope
      * Pan position
      * Filter frequency
      * Reverse flag
    
    === PROBABILITY ENGINE ===
    
    Markov Chain system:
    - 8×8 transition matrix
    - Each state has probability parameters
    - State transitions every 100-500ms (mutation rate)
    
    === MUTATION SYSTEM ===
    
    Pattern evolution:
    1. Current state → Probability table
    2. Random selection based on weights
    3. Smooth crossfade to new state
    4. Update transition probabilities
    
    === 8 RANDOMIZATION MODES ===
    
    0. GENTLE - Subtle variations
    1. MODERATE - Clear rhythmic variations
    2. WILD - Extreme chaos
    3. GLITCH - Digital artifacts
    4. RHYTHMIC - Quantized to grid
    5. MELODIC - Musical intervals
    6. AMBIENT - Long, evolving textures
    7. INDUSTRIAL - Harsh, metallic
    
    ═══════════════════════════════════════════════════════════════
    INSPIRED BY
    ═══════════════════════════════════════════════════════════════
    
    Algorithms:
    - Curtis Roads: Granular Synthesis
    - Iannis Xenakis: Stochastic composition
    - Brian Eno: Generative music systems
    
    Hardware:
    - Mutable Instruments Clouds/Beads
    - Mungo d0 Universal CV Generator
    - Soma Laboratory LYRA-8
    
    Artists:
    - Autechre (algorithmic composition)
    - Aphex Twin (glitch aesthetics)
    - Alva Noto (digital artifacts)
    
*/

#include "unit_revfx.h"
#include "utils/float_math.h"
#include "osc_api.h"
#include "fx_api.h"

#define MAX_GRAINS 32
#define GRAIN_BUFFER_SIZE 2048
#define CAPTURE_BUFFER_SIZE 96000  // 2 seconds @ 48kHz
#define PROB_MATRIX_SIZE 8

// Grain structure
struct Grain {
    bool active;
    
    // Buffer playback
    uint32_t start_pos;     // Position in capture buffer
    uint32_t current_pos;   // Current read position
    uint32_t grain_length;  // Length in samples
    uint32_t envelope_pos;  // Position in envelope
    
    // Randomization
    float pitch;            // Playback speed (0.5 - 2.0)
    float pan;              // -1.0 to 1.0
    bool reverse;           // Play backwards
    
    // Filter
    float filter_freq;
    float filter_q;
    float filter_z1_l, filter_z2_l;
    float filter_z1_r, filter_z2_r;
    
    // Envelope
    float envelope;
    float volume;
};

static Grain s_grains[MAX_GRAINS];

// Capture buffer pointers (allocated in SDRAM)
static float *s_capture_l;
static float *s_capture_r;
static uint32_t s_capture_write;

// Probability engine
struct ProbState {
    float trigger_prob;      // 0.0 - 1.0
    float pitch_range;       // ±semitones
    float grain_size_min;    // ms
    float grain_size_max;    // ms
    float pan_spread;        // 0.0 - 1.0
    float filter_min;        // Hz
    float filter_max;        // Hz
    float reverse_prob;      // 0.0 - 1.0
};

static ProbState s_prob_states[PROB_MATRIX_SIZE];
static float s_transition_matrix[PROB_MATRIX_SIZE][PROB_MATRIX_SIZE];
static uint8_t s_current_state;
static uint8_t s_target_state;
static float s_state_crossfade;

// Pattern memory
static float s_pattern_snapshots[8][PROB_MATRIX_SIZE][PROB_MATRIX_SIZE];

// Random state
static uint32_t s_random_seed;

// Mutation
static uint32_t s_mutation_counter;
static uint32_t s_mutation_interval;

// Parameters
static float s_density;
static float s_chaos_amount;
static float s_mutation_rate;
static float s_grain_size_base;
static float s_pitch_range;
static float s_feedback_amount;
static float s_mix;
static uint8_t s_mode;
static uint8_t s_pattern_select;
static bool s_freeze;

static uint32_t s_sample_counter;
static uint32_t s_trigger_counter;  // FIXED: Rate limit grain triggering

// Fast random (Xorshift)
inline uint32_t xorshift32() {
    s_random_seed ^= s_random_seed << 13;
    s_random_seed ^= s_random_seed >> 17;
    s_random_seed ^= s_random_seed << 5;
    return s_random_seed;
}

inline float random_float() {
    return (float)(xorshift32() % 10000) / 10000.f;
}

inline float random_range(float min, float max) {
    return min + random_float() * (max - min);
}

inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// Hann window envelope (osc_cosf expects phase in [0,1] range)
inline float hann_window(float phase) {
    if (phase < 0.f) phase = 0.f;
    if (phase >= 1.f) phase = 0.999f;
    return 0.5f * (1.f - osc_cosf(phase));
}

// Band-pass filter (simplified)
inline void process_grain_filter(Grain *g, float *in_l, float *in_r) {
    const float PI_VAL = 3.14159265f;  // FIXED: Use different name (PI is macro in arm_math.h)
    float w = 2.f * PI_VAL * g->filter_freq / 48000.f;
    if (w > PI_VAL) w = PI_VAL;
    
    float bw = w / g->filter_q;
    if (bw < 0.001f) bw = 0.001f;
    
    float r = 1.f - bw * 0.5f;
    if (r < 0.1f) r = 0.1f;
    
    float phase = w / (2.f * 3.14159265f);
    if (phase >= 1.f) phase -= 1.f;
    if (phase < 0.f) phase += 1.f;
    
    float cos_w = osc_cosf(phase);
    float k = (1.f - 2.f * r * cos_w + r * r) / (2.f - 2.f * cos_w);
    if (k < 0.f) k = 0.f;
    if (k > 1.f) k = 1.f;
    
    float a0 = 1.f - k;
    float a1 = 2.f * (k - r) * cos_w;
    float a2 = r * r - k;
    float b1 = 2.f * r * cos_w;
    float b2 = -r * r;
    
    // Left
    float out_l = a0 * (*in_l) + a1 * g->filter_z1_l + a2 * g->filter_z2_l 
                  - b1 * g->filter_z1_l - b2 * g->filter_z2_l;
    g->filter_z2_l = g->filter_z1_l;
    g->filter_z1_l = *in_l;
    *in_l = out_l;
    
    // Right
    float out_r = a0 * (*in_r) + a1 * g->filter_z1_r + a2 * g->filter_z2_r 
                  - b1 * g->filter_z1_r - b2 * g->filter_z2_r;
    g->filter_z2_r = g->filter_z1_r;
    g->filter_z1_r = *in_r;
    *in_r = out_r;
}

// Initialize probability states based on mode
void init_prob_states() {
    switch (s_mode) {
        case 0: // GENTLE
            for (int i = 0; i < PROB_MATRIX_SIZE; i++) {
                s_prob_states[i].trigger_prob = 0.3f + random_float() * 0.2f;
                s_prob_states[i].pitch_range = 2.f;
                s_prob_states[i].grain_size_min = 30.f;
                s_prob_states[i].grain_size_max = 100.f;
                s_prob_states[i].pan_spread = 0.3f;
                s_prob_states[i].filter_min = 200.f;
                s_prob_states[i].filter_max = 5000.f;
                s_prob_states[i].reverse_prob = 0.1f;
            }
            break;
        
        case 1: // MODERATE
            for (int i = 0; i < PROB_MATRIX_SIZE; i++) {
                s_prob_states[i].trigger_prob = 0.4f + random_float() * 0.3f;
                s_prob_states[i].pitch_range = 5.f;
                s_prob_states[i].grain_size_min = 20.f;
                s_prob_states[i].grain_size_max = 150.f;
                s_prob_states[i].pan_spread = 0.6f;
                s_prob_states[i].filter_min = 100.f;
                s_prob_states[i].filter_max = 10000.f;
                s_prob_states[i].reverse_prob = 0.3f;
            }
            break;
        
        case 2: // WILD
            for (int i = 0; i < PROB_MATRIX_SIZE; i++) {
                s_prob_states[i].trigger_prob = 0.5f + random_float() * 0.5f;
                s_prob_states[i].pitch_range = 24.f;
                s_prob_states[i].grain_size_min = 5.f;
                s_prob_states[i].grain_size_max = 300.f;
                s_prob_states[i].pan_spread = 1.f;
                s_prob_states[i].filter_min = 20.f;
                s_prob_states[i].filter_max = 20000.f;
                s_prob_states[i].reverse_prob = 0.5f;
            }
            break;
        
        case 3: // GLITCH
            for (int i = 0; i < PROB_MATRIX_SIZE; i++) {
                s_prob_states[i].trigger_prob = 0.7f + random_float() * 0.3f;
                s_prob_states[i].pitch_range = 12.f;
                s_prob_states[i].grain_size_min = 2.f;
                s_prob_states[i].grain_size_max = 50.f;
                s_prob_states[i].pan_spread = 0.8f;
                s_prob_states[i].filter_min = 500.f;
                s_prob_states[i].filter_max = 15000.f;
                s_prob_states[i].reverse_prob = 0.4f;
            }
            break;
        
        case 4: // RHYTHMIC
            for (int i = 0; i < PROB_MATRIX_SIZE; i++) {
                s_prob_states[i].trigger_prob = 0.6f;
                s_prob_states[i].pitch_range = 7.f;
                s_prob_states[i].grain_size_min = 50.f;
                s_prob_states[i].grain_size_max = 100.f;
                s_prob_states[i].pan_spread = 0.4f;
                s_prob_states[i].filter_min = 300.f;
                s_prob_states[i].filter_max = 8000.f;
                s_prob_states[i].reverse_prob = 0.2f;
            }
            break;
        
        case 5: // MELODIC
            for (int i = 0; i < PROB_MATRIX_SIZE; i++) {
                s_prob_states[i].trigger_prob = 0.4f + random_float() * 0.2f;
                s_prob_states[i].pitch_range = 12.f;
                s_prob_states[i].grain_size_min = 40.f;
                s_prob_states[i].grain_size_max = 120.f;
                s_prob_states[i].pan_spread = 0.5f;
                s_prob_states[i].filter_min = 400.f;
                s_prob_states[i].filter_max = 6000.f;
                s_prob_states[i].reverse_prob = 0.15f;
            }
            break;
        
        case 6: // AMBIENT
            for (int i = 0; i < PROB_MATRIX_SIZE; i++) {
                s_prob_states[i].trigger_prob = 0.2f + random_float() * 0.2f;
                s_prob_states[i].pitch_range = 7.f;
                s_prob_states[i].grain_size_min = 200.f;
                s_prob_states[i].grain_size_max = 500.f;
                s_prob_states[i].pan_spread = 0.7f;
                s_prob_states[i].filter_min = 150.f;
                s_prob_states[i].filter_max = 4000.f;
                s_prob_states[i].reverse_prob = 0.25f;
            }
            break;
        
        case 7: // INDUSTRIAL
            for (int i = 0; i < PROB_MATRIX_SIZE; i++) {
                s_prob_states[i].trigger_prob = 0.8f + random_float() * 0.2f;
                s_prob_states[i].pitch_range = 18.f;
                s_prob_states[i].grain_size_min = 10.f;
                s_prob_states[i].grain_size_max = 80.f;
                s_prob_states[i].pan_spread = 0.9f;
                s_prob_states[i].filter_min = 800.f;
                s_prob_states[i].filter_max = 18000.f;
                s_prob_states[i].reverse_prob = 0.45f;
            }
            break;
    }
    
    // Initialize transition matrix (random walk)
    for (int i = 0; i < PROB_MATRIX_SIZE; i++) {
        float total = 0.f;
        for (int j = 0; j < PROB_MATRIX_SIZE; j++) {
            s_transition_matrix[i][j] = random_float();
            total += s_transition_matrix[i][j];
        }
        // Normalize
        if (total > 0.f) {
            for (int j = 0; j < PROB_MATRIX_SIZE; j++) {
                s_transition_matrix[i][j] /= total;
            }
        }
    }
}

// Trigger new grain
void trigger_grain() {
    // Find free grain
    int free_grain = -1;
    for (int i = 0; i < MAX_GRAINS; i++) {
        if (!s_grains[i].active) {
            free_grain = i;
            break;
        }
    }
    
    if (free_grain == -1) {
        // Steal oldest
        free_grain = 0;
    }
    
    Grain *g = &s_grains[free_grain];
    
    // Get current probability state
    ProbState *state = &s_prob_states[s_current_state];
    
    // Check trigger probability
    if (random_float() > state->trigger_prob * s_density) {
        return;  // Don't trigger
    }
    
    // Random start position in capture buffer
    g->start_pos = xorshift32() % (CAPTURE_BUFFER_SIZE - GRAIN_BUFFER_SIZE);
    g->current_pos = 0;
    
    // Random grain length
    float grain_ms = random_range(state->grain_size_min, state->grain_size_max);
    grain_ms *= s_grain_size_base;
    g->grain_length = (uint32_t)(grain_ms * 48.f);  // ms to samples
    g->grain_length = clipminmaxi32(100, g->grain_length, GRAIN_BUFFER_SIZE);
    
    // Random pitch
    float pitch_semitones = random_range(-state->pitch_range, state->pitch_range) * s_pitch_range;
    g->pitch = fx_pow2f(pitch_semitones / 12.f);  // FIXED: Use fx_pow2f instead of fastpow2f
    g->pitch = clipminmaxf(0.25f, g->pitch, 4.f);
    
    // Random pan
    g->pan = random_range(-state->pan_spread, state->pan_spread);
    
    // Random reverse
    g->reverse = (random_float() < state->reverse_prob);
    
    // Random filter
    g->filter_freq = random_range(state->filter_min, state->filter_max);
    g->filter_q = random_range(0.5f, 10.f);
    g->filter_z1_l = g->filter_z2_l = 0.f;
    g->filter_z1_r = g->filter_z2_r = 0.f;
    
    // Init envelope
    g->envelope_pos = 0;
    g->volume = 0.7f + random_float() * 0.3f;
    
    g->active = true;
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;

    // Check SDRAM allocation available
    if (!desc->hooks.sdram_alloc) return k_unit_err_memory;

    // Allocate capture buffers in SDRAM (2 channels × 96000 samples)
    uint32_t total_samples = 2 * CAPTURE_BUFFER_SIZE;
    float *sdram_buffer = (float *)desc->hooks.sdram_alloc(total_samples * sizeof(float));
    if (!sdram_buffer) return k_unit_err_memory;

    // Clear buffer (FIXED: Manual clear instead of std::fill)
    for (uint32_t i = 0; i < total_samples; i++) {
        sdram_buffer[i] = 0.f;
    }

    // Assign buffer pointers
    s_capture_l = sdram_buffer;
    s_capture_r = sdram_buffer + CAPTURE_BUFFER_SIZE;
    s_capture_write = 0;

    // Init grains
    for (int i = 0; i < MAX_GRAINS; i++) {
        s_grains[i].active = false;
    }
    
    // Init random
    s_random_seed = 0x12345678;
    
    // Init probability states
    s_current_state = 0;
    s_target_state = 0;
    s_state_crossfade = 0.f;
    
    s_mutation_counter = 0;
    s_mutation_interval = 24000;  // 500ms
    
    // Init parameters
    s_density = 0.75f;
    s_chaos_amount = 0.6f;
    s_mutation_rate = 0.5f;
    s_grain_size_base = 0.8f;
    s_pitch_range = 0.3f;
    s_feedback_amount = 0.65f;
    s_mix = 0.4f;
    s_mode = 1;
    s_pattern_select = 0;
    s_freeze = false;
    
    init_prob_states();
    
    s_sample_counter = 0;
    s_trigger_counter = 0;  // FIXED: Initialize trigger counter

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    for (int i = 0; i < MAX_GRAINS; i++) {
        s_grains[i].active = false;
    }
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    const float *in_ptr = in;
    float *out_ptr = out;
    
    for (uint32_t f = 0; f < frames; f++) {
        float in_l = in_ptr[0];
        float in_r = in_ptr[1];
        
        // Write to capture buffer
        s_capture_l[s_capture_write] = in_l;
        s_capture_r[s_capture_write] = in_r;
        s_capture_write = (s_capture_write + 1) % CAPTURE_BUFFER_SIZE;
        
        // Mutation (state evolution)
        if (!s_freeze) {
            s_mutation_counter++;
            s_mutation_interval = (uint32_t)(2400.f + (1.f - s_mutation_rate) * 45600.f);
            
            if (s_mutation_counter >= s_mutation_interval) {
                s_mutation_counter = 0;
                
                // Choose next state based on transition matrix
                float rnd = random_float();
                float cumulative = 0.f;
                for (int i = 0; i < PROB_MATRIX_SIZE; i++) {
                    cumulative += s_transition_matrix[s_current_state][i];
                    if (rnd < cumulative) {
                        s_target_state = i;
                        break;
                    }
                }
                
                s_current_state = s_target_state;
            }
        }
        
        // Trigger new grains (FIXED: Rate-limited to prevent feedback/whistle)
        s_trigger_counter++;
        // Calculate trigger interval based on density (10-1000 samples = 48Hz - 0.48Hz)
        uint32_t trigger_interval = (uint32_t)(10.f + (1.f - s_density) * 990.f);
        
        if (s_trigger_counter >= trigger_interval) {
            s_trigger_counter = 0;
            // Check probability (now much lower per check)
            if (random_float() < s_density * 0.3f) {
                trigger_grain();
            }
        }
        
        // Render all active grains
        float grain_sum_l = 0.f;
        float grain_sum_r = 0.f;
        int active_count = 0;
        
        for (int i = 0; i < MAX_GRAINS; i++) {
            Grain *g = &s_grains[i];
            if (!g->active) continue;
            
            // Read from capture buffer (with pitch shift)
            uint32_t read_pos;
            
            if (g->reverse) {
                // FIXED: Reverse playback - read backwards
                if (g->current_pos >= g->grain_length) {
                    g->active = false;
                    continue;
                }
                read_pos = g->start_pos + g->grain_length - 1 - (uint32_t)((float)g->current_pos * g->pitch);
                if (read_pos < g->start_pos) {
                    g->active = false;
                    continue;
                }
            } else {
                // Forward playback
                read_pos = g->start_pos + (uint32_t)((float)g->current_pos * g->pitch);
                if (read_pos >= g->start_pos + g->grain_length) {
                    g->active = false;
                    continue;
                }
            }
            
            // Wrap around capture buffer
            read_pos = read_pos % CAPTURE_BUFFER_SIZE;
            
            float sample_l = s_capture_l[read_pos];
            float sample_r = s_capture_r[read_pos];
            
            // Filter
            process_grain_filter(g, &sample_l, &sample_r);
            
            // Envelope
            float env_phase = (float)g->envelope_pos / (float)g->grain_length;
            g->envelope = hann_window(env_phase);
            
            // Pan
            float gain_l = (1.f - g->pan) * 0.5f;
            float gain_r = (1.f + g->pan) * 0.5f;
            
            grain_sum_l += sample_l * g->envelope * g->volume * gain_l;
            grain_sum_r += sample_r * g->envelope * g->volume * gain_r;
            
            g->current_pos++;
            g->envelope_pos++;
            active_count++;
        }
        
        // Normalize (FIXED: Better normalization to prevent feedback)
        if (active_count > 0) {
            float norm = 1.f / (1.f + (float)active_count * 0.15f);
            grain_sum_l *= norm;
            grain_sum_r *= norm;
        }
        
        // FIXED: Additional safety limit to prevent feedback/whistle
        grain_sum_l = clipminmaxf(-0.8f, grain_sum_l, 0.8f);
        grain_sum_r = clipminmaxf(-0.8f, grain_sum_r, 0.8f);
        
        // Feedback
        grain_sum_l = grain_sum_l + in_l * s_feedback_amount;
        grain_sum_r = grain_sum_r + in_r * s_feedback_amount;
        
        // Mix
        out_ptr[0] = in_l * (1.f - s_mix) + grain_sum_l * s_mix;
        out_ptr[1] = in_r * (1.f - s_mix) + grain_sum_r * s_mix;
        
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
    
    switch (id) {
        case 0: s_density = valf; break;
        case 1: s_chaos_amount = valf; break;
        case 2: s_mutation_rate = valf; break;
        case 3: s_grain_size_base = valf; break;
        case 4: s_pitch_range = valf; break;
        case 5: s_feedback_amount = valf; break;
        case 6: s_mix = valf; break;
        case 7: 
            s_mode = value;
            init_prob_states();
            break;
        case 8: s_pattern_select = value; break;
        case 9: s_freeze = (value > 0); break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_density * 1023.f);
        case 1: return (int32_t)(s_chaos_amount * 1023.f);
        case 2: return (int32_t)(s_mutation_rate * 1023.f);
        case 3: return (int32_t)(s_grain_size_base * 1023.f);
        case 4: return (int32_t)(s_pitch_range * 1023.f);
        case 5: return (int32_t)(s_feedback_amount * 1023.f);
        case 6: return (int32_t)(s_mix * 1023.f);
        case 7: return s_mode;
        case 8: return s_pattern_select;
        case 9: return s_freeze ? 1 : 0;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 7) {
        static const char *mode_names[] = {
            "GENTLE", "MODERATE", "WILD", "GLITCH",
            "RHYTHM", "MELODIC", "AMBIENT", "INDUSTR"
        };
        if (value >= 0 && value < 8) return mode_names[value];
    }
    if (id == 8) {
        static const char *pattern_names[] = {
            "PAT1", "PAT2", "PAT3", "PAT4", "PAT5", "PAT6", "PAT7", "PAT8"
        };
        if (value >= 0 && value < 8) return pattern_names[value];
    }
    if (id == 9) {
        return value ? "FREEZE" : "EVOLVE";
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {}
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {}

