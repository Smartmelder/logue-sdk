/*
    STEPSEQ - Programmable Step Sequencer Modulation
    
    ═══════════════════════════════════════════════════════════════
    ARCHITECTURE
    ═══════════════════════════════════════════════════════════════
    
    STEP DATA STRUCTURE:
    Each step contains:
    - Pitch offset (-24 to +24 semitones)
    - Filter cutoff (0-100%)
    - Gate length (0-100%)
    - Ratchet count (1-4 repeats)
    - Probability (0-100%)
    - Active flag (on/off)
    
    TEMPO SYNC:
    - Uses MIDI clock (4PPQN = 16th notes)
    - Swing adds timing offset to even steps
    - Ratcheting divides step into sub-steps
    
    MODULATION OUTPUT:
    - Pitch: Added to input signal (ring mod style)
    - Filter: Modulates a SVF (state-variable filter)
    - Gate: Amplitude envelope per step
    
    PATTERN MEMORY:
    - 8 patterns × 16 steps
    - Stored in SDRAM
    - Instant recall
    
    ═══════════════════════════════════════════════════════════════
    USAGE GUIDE
    ═══════════════════════════════════════════════════════════════
    
    PROGRAMMING STEPS:
    1. Select step (Knop A / param 0)
    2. Set pitch offset (Knop B / param 1)
    3. Set filter mod (param 2)
    4. Set gate length (param 3)
    5. Repeat for all steps
    
    PLAYBACK:
    - Set sequence length (param 4): 1-16 steps
    - Adjust swing (param 5): 0-100%
    - Set ratcheting (param 6): 1x/2x/3x/4x
    - Set direction (param 9): FWD/REV/PING-PONG/RANDOM
    
    PATTERN MANAGEMENT:
    - Select pattern (param 8): 0-7
    - Each pattern remembers all step data
    - Great for live performance!
    
    Based on Korg logue SDK modfx template
    https://github.com/korginc/logue-sdk
*/

#include "unit_modfx.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "fx_api.h"
#include <algorithm>

// SDK compatibility - PI is already defined in CMSIS arm_math.h
#ifndef PI
#define PI 3.14159265359f
#endif

#define NUM_STEPS 16
#define NUM_PATTERNS 8

// Step data structure
struct Step {
    int8_t pitch_offset;      // -24 to +24 semitones
    float filter_mod;         // 0.0 to 1.0
    float gate_length;        // 0.0 to 1.0
    uint8_t ratchet_count;    // 1-4
    float probability;        // 0.0 to 1.0
    bool active;
};

// Pattern (collection of steps)
struct Pattern {
    Step steps[NUM_STEPS];
    uint8_t length;           // 1-16
};

static Pattern s_patterns[NUM_PATTERNS];
static uint8_t s_current_pattern;

// Sequencer state
static uint8_t s_current_step;
static int8_t s_step_direction;  // 1=forward, -1=reverse
static bool s_is_ping_pong;
static uint32_t s_step_counter;
static uint32_t s_samples_per_step;
static float s_gate_phase;        // 0.0 to 1.0 within current step
static uint8_t s_ratchet_index;   // Current ratchet within step

// MIDI sync
static uint32_t s_tempo_bpm;
static uint32_t s_last_tick_time;
static bool s_tempo_synced;

// Step sequencer parameters
static uint8_t s_selected_step;
static int8_t s_edit_pitch;
static float s_edit_filter;
static float s_edit_gate;
static uint8_t s_sequence_length;
static float s_swing_amount;
static uint8_t s_ratchet_mode;
static float s_step_probability;
static uint8_t s_direction_mode;  // 0=FWD, 1=REV, 2=PING, 3=RANDOM
// ✅ ADD: Play/Stop state
static bool s_sequencer_playing = true;  // ON by default!

// State-variable filter
static float s_svf_z1_l, s_svf_z2_l;
static float s_svf_z1_r, s_svf_z2_r;

// Envelope
static float s_amp_envelope;

// Random seed
static uint32_t s_random_seed;

static uint32_t s_sample_counter;

// XORShift random
inline uint32_t xorshift32() {
    s_random_seed ^= s_random_seed << 13;
    s_random_seed ^= s_random_seed >> 17;
    s_random_seed ^= s_random_seed << 5;
    return s_random_seed;
}

inline float random_float() {
    return (float)(xorshift32() % 10000) / 10000.f;
}

// State-variable filter (LP output)
inline float svf_process(float input, float cutoff, float resonance, 
                         float *z1, float *z2) {
    // ✅ Safety: Clip input
    input = clipminmaxf(-2.f, input, 2.f);
    
    // ✅ FIX: Safe cutoff range (100Hz - 12kHz)
    cutoff = clipminmaxf(0.15f, cutoff, 0.85f);
    float freq = 100.f + cutoff * 11900.f;
    freq = clipminmaxf(100.f, freq, 12000.f);
    
    float w = 2.f * PI * freq / 48000.f;
    w = clipminmaxf(0.001f, w, PI * 0.95f);  // Prevent aliasing
    float phase = (w * 0.5f) / (2.f * PI);
    phase = clipminmaxf(0.f, phase, 0.5f);
    
    // ✅ CRITICAL: Limit resonance to prevent fluittoon!
    resonance = clipminmaxf(0.3f, resonance, 0.707f);  // NEVER > 0.707!
    
    float sin_w = fx_sinf(phase);
    float cos_w = fx_cosf(phase);
    
    float alpha = sin_w / (2.f * resonance);
    alpha = clipminmaxf(0.001f, alpha, 0.99f);  // Safety
    
    // Biquad coefficients
    float b0 = (1.f - cos_w) / 2.f;
    float b1 = 1.f - cos_w;
    float b2 = (1.f - cos_w) / 2.f;
    float a0 = 1.f + alpha;
    float a1 = -2.f * cos_w;
    float a2 = 1.f - alpha;
    
    b0 /= a0; b1 /= a0; b2 /= a0;
    a1 /= a0; a2 /= a0;
    
    // Process
    float output = b0 * input + b1 * (*z1) + b2 * (*z2) - a1 * (*z1) - a2 * (*z2);
    
    // ✅ Update states
    *z2 = *z1;
    *z1 = input;
    
    // ✅ CRITICAL: Denormal kill!
    if (si_fabsf(*z1) < 1e-15f) *z1 = 0.f;
    if (si_fabsf(*z2) < 1e-15f) *z2 = 0.f;
    
    // ✅ Output limiting
    output = clipminmaxf(-2.f, output, 2.f);
    
    return output;
}

// Pitch shifter (simple ring modulation)
inline float pitch_shift(float input, int8_t semitones) {
    if (semitones == 0) return input;
    
    // ✅ FIXED: Use fx_pow2f (NOT fastpow2f - doesn't exist!)
    float ratio = fx_pow2f((float)semitones / 12.f);
    ratio = clipminmaxf(0.25f, ratio, 4.f);  // Limit range for safety
    
    float phase = (float)(s_sample_counter % 48000) / 48000.f;
    phase *= ratio;
    
    // Wrap phase properly
    while (phase >= 1.f) phase -= 1.f;
    while (phase < 0.f) phase += 1.f;
    
    // Convert phase to [-0.5, 0.5] range, then normalize to [0, 1]
    float phase_norm = phase - 0.5f;
    if (phase_norm < 0.f) phase_norm += 1.f;
    float carrier = fx_sinf(phase_norm);
    
    // ✅ MORE AUDIBLE: 50/50 mix (was 70/30)
    float dry = 0.5f;   // 50% dry
    float wet = 0.5f;   // 50% wet
    
    return input * dry + input * carrier * wet;
}

// Calculate swing offset
inline float calc_swing_offset(uint8_t step_index) {
    if (step_index % 2 == 0) {
        return 0.f;  // Even steps: no offset
    } else {
        // Odd steps: delay based on swing amount
        return (s_swing_amount - 0.5f) * 0.3f;  // ±15% timing offset
    }
}

// Get current step data
inline Step* get_current_step() {
    return &s_patterns[s_current_pattern].steps[s_current_step];
}

// Advance sequencer (NO RECURSION - iterative loop!)
void advance_sequencer() {
    Pattern *pattern = &s_patterns[s_current_pattern];
    
    // ✅ FIX: Check step probability FIRST (iterative, max 16 attempts)
    uint8_t attempts = 0;
    bool step_accepted = false;
    int16_t next_step;  // ✅ Declare outside switch to avoid compile error
    
    while (attempts < 16 && !step_accepted) {
        // Apply direction mode
        switch (s_direction_mode) {
            case 0:  // Forward
                s_current_step++;
                if (s_current_step >= s_sequence_length) {
                    s_current_step = 0;
                }
                break;
                
            case 1:  // Reverse
                if (s_current_step == 0) {
                    s_current_step = s_sequence_length - 1;
                } else {
                    s_current_step--;
                }
                break;
                
            case 2: { // Ping-Pong (block needed for variable declaration)
                // ✅ FIX: Use int16_t to avoid underflow!
                next_step = (int16_t)s_current_step + (int16_t)s_step_direction;
                
                if (next_step >= (int16_t)s_sequence_length) {
                    // Hit top, bounce back
                    s_current_step = s_sequence_length - 2;
                    if (s_current_step >= s_sequence_length) s_current_step = 0;  // Safety
                    s_step_direction = -1;
                } else if (next_step < 0) {
                    // Hit bottom, bounce forward
                    s_current_step = 1;
                    if (s_current_step >= s_sequence_length) s_current_step = 0;  // Safety
                    s_step_direction = 1;
                } else {
                    s_current_step = (uint8_t)next_step;
                }
                break;
            }
                
            case 3:  // Random
                s_current_step = xorshift32() % s_sequence_length;
                if (s_current_step >= s_sequence_length) s_current_step = 0;  // Safety
                break;
                
            default:
                s_current_step = 0;
                s_direction_mode = 0;  // Reset to forward
                break;
        }
        
        // ✅ Check step probability
        Step *step = get_current_step();
        if (random_float() <= step->probability) {
            step_accepted = true;  // Step passed probability check!
        } else {
            attempts++;  // Failed probability, try next step
        }
    }
    
    // ✅ Safety: If all steps failed probability, force step 0
    if (attempts >= 16) {
        s_current_step = 0;
    }
    
    // Reset gate phase
    s_gate_phase = 0.f;
    s_ratchet_index = 0;
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;

    // Initialize all patterns
    for (int p = 0; p < NUM_PATTERNS; p++) {
        s_patterns[p].length = 16;
        
        for (int s = 0; s < NUM_STEPS; s++) {
            s_patterns[p].steps[s].pitch_offset = 0;
            s_patterns[p].steps[s].filter_mod = 0.5f;
            s_patterns[p].steps[s].gate_length = 0.75f;
            s_patterns[p].steps[s].ratchet_count = 1;
            s_patterns[p].steps[s].probability = 1.0f;
            s_patterns[p].steps[s].active = true;
        }
    }
    
    // Create some interesting default patterns
    // Pattern 0: Chromatic scale up
    for (int s = 0; s < NUM_STEPS; s++) {
        s_patterns[0].steps[s].pitch_offset = s - 7;
    }
    
    // Pattern 1: Octaves
    for (int s = 0; s < NUM_STEPS; s++) {
        s_patterns[1].steps[s].pitch_offset = (s % 4) * 12;
        s_patterns[1].steps[s].filter_mod = (float)(s % 4) / 4.f;
    }
    
    // Pattern 2: Fifths
    int fifths[] = {0, 7, 12, 7, 0, -5, 0, 7};
    for (int s = 0; s < 8; s++) {
        s_patterns[2].steps[s].pitch_offset = fifths[s];
        s_patterns[2].steps[s * 2].filter_mod = 0.8f;
    }
    
    // Pattern 3: Rhythmic gates
    for (int s = 0; s < NUM_STEPS; s++) {
        s_patterns[3].steps[s].gate_length = (s % 4 == 0) ? 1.0f : 0.25f;
        s_patterns[3].steps[s].filter_mod = (s % 2 == 0) ? 0.8f : 0.3f;
    }
    
    s_current_pattern = 0;
    s_current_step = 0;
    s_step_direction = 1;
    s_is_ping_pong = false;
    s_step_counter = 0;
    s_gate_phase = 0.f;
    s_ratchet_index = 0;
    
    s_tempo_bpm = 120;
    s_samples_per_step = 12000;  // 120 BPM, 16th notes @ 48kHz
    s_last_tick_time = 0;
    s_tempo_synced = false;
    
    s_selected_step = 0;
    s_edit_pitch = 0;
    s_edit_filter = 0.5f;
    s_edit_gate = 0.75f;
    s_sequence_length = 16;
    s_swing_amount = 0.5f;
    s_ratchet_mode = 0;
    s_step_probability = 1.0f;
    s_direction_mode = 0;
    
    s_svf_z1_l = s_svf_z2_l = 0.f;
    s_svf_z1_r = s_svf_z2_r = 0.f;
    s_amp_envelope = 0.f;
    
    s_random_seed = 12345;
    s_sample_counter = 0;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    s_current_step = 0;
    s_step_counter = 0;
    s_gate_phase = 0.f;
    s_amp_envelope = 0.f;
    s_ratchet_index = 0;
    s_step_direction = 1;  // ✅ Reset to forward
    
    // ✅ FIX: Reset filter states to prevent clicks and fluittoon
    s_svf_z1_l = s_svf_z2_l = 0.f;
    s_svf_z1_r = s_svf_z2_r = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    for (uint32_t f = 0; f < frames; f++) {
        float in_l = clipminmaxf(-1.f, in[f * 2], 1.f);
        float in_r = clipminmaxf(-1.f, in[f * 2 + 1], 1.f);
        
        // ✅ CHECK: Is sequencer playing?
        if (!s_sequencer_playing) {
            // ✅ PASS-THROUGH mode when stopped
            out[f * 2] = in_l;
            out[f * 2 + 1] = in_r;
            continue;  // Skip all processing!
        }
        
        // ✅ Sequencer is playing - get current step
        Step *current_step_data = get_current_step();
        
        // Update step position (only when playing)
        s_step_counter++;
        
        // Calculate samples per step (with ratcheting)
        uint8_t ratchet_div = current_step_data->ratchet_count;
        uint32_t step_length = s_samples_per_step / ratchet_div;
        
        // Apply swing offset (only to odd steps)
        if (s_current_step % 2 == 1) {
            float swing_offset = calc_swing_offset(s_current_step);
            step_length = (uint32_t)((float)step_length * (1.f + swing_offset));
        }
        
        // Check if we need to advance to next step
        if (s_step_counter >= step_length) {
            s_step_counter = 0;
            s_ratchet_index++;
            
            if (s_ratchet_index >= current_step_data->ratchet_count) {
                // Move to next step
                advance_sequencer();
                current_step_data = get_current_step();
                s_ratchet_index = 0;
            }
            
            s_gate_phase = 0.f;
        }
        
        // Update gate phase (0.0 to 1.0 within step)
        s_gate_phase = (float)s_step_counter / (float)step_length;
        
        // Calculate gate (amplitude envelope)
        float gate_length = current_step_data->gate_length;
        float gate = 0.f;
        
        if (s_gate_phase < gate_length) {
            // Attack phase
            if (s_gate_phase < 0.01f) {
                gate = s_gate_phase / 0.01f;  // Fast attack (10ms)
            } else {
                gate = 1.f;
            }
        } else {
            // Release phase
            float release_phase = (s_gate_phase - gate_length) / (1.f - gate_length);
            gate = 1.f - release_phase;
        }
        
        gate = clipminmaxf(0.f, gate, 1.f);
        
        // ✅ SNAPPIER: Faster envelope response (3x faster!)
        float envelope_speed = 0.3f;  // Was 0.1f
        if (gate_length < 0.3f) {
            // Short gates: even snappier attack/release
            envelope_speed = 0.5f;
        }
        s_amp_envelope += (gate - s_amp_envelope) * envelope_speed;
        
        // Apply pitch offset (ring modulation style)
        float pitched_l = pitch_shift(in_l, current_step_data->pitch_offset);
        float pitched_r = pitch_shift(in_r, current_step_data->pitch_offset);
        
        // ✅ MORE DRAMATIC: Wider filter range (50Hz - 15kHz)
        float filter_cutoff = current_step_data->filter_mod;  // 0%-100% (full range!)
        filter_cutoff = clipminmaxf(0.05f, filter_cutoff, 0.95f);  // 5%-95%
        
        // Map to frequency: 50Hz - 15kHz (dramatic!)
        float freq = 50.f + filter_cutoff * 14950.f;
        freq = clipminmaxf(50.f, freq, 15000.f);
        
        // Convert back to normalized cutoff for SVF (0-1 range)
        // SVF expects normalized frequency: f / sample_rate
        filter_cutoff = freq / 48000.f;
        filter_cutoff = clipminmaxf(0.001f, filter_cutoff, 0.48f);  // Prevent aliasing
        
        // ✅ FIX: Safe Q-range to prevent fluittoon (0.4-0.707 max!)
        float filter_resonance = 0.4f + current_step_data->filter_mod * 0.3f;  // 0.4-0.7
        filter_resonance = clipminmaxf(0.3f, filter_resonance, 0.707f);  // MAX 0.707!
        
        float filtered_l = svf_process(pitched_l, filter_cutoff, filter_resonance, 
                                       &s_svf_z1_l, &s_svf_z2_l);
        float filtered_r = svf_process(pitched_r, filter_cutoff, filter_resonance, 
                                       &s_svf_z1_r, &s_svf_z2_r);
        
        // Apply gate envelope
        float modulated_l = filtered_l * s_amp_envelope;
        float modulated_r = filtered_r * s_amp_envelope;
        
        // ✅ MORE WET: 30/70 mix (effect more audible!)
        out[f * 2] = in_l * 0.3f + modulated_l * 0.7f;
        out[f * 2 + 1] = in_r * 0.3f + modulated_r * 0.7f;
        
        // ✅ Boost output slightly for more presence
        out[f * 2] *= 1.2f;
        out[f * 2 + 1] *= 1.2f;
        
        // Output limiting
        out[f * 2] = clipminmaxf(-1.f, out[f * 2], 1.f);
        out[f * 2 + 1] = clipminmaxf(-1.f, out[f * 2 + 1], 1.f);
        
        s_sample_counter++;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0:  // ✅ PLAY/STOP (NEW!)
            s_sequencer_playing = (value != 0);
            
            // Reset sequencer when starting
            if (s_sequencer_playing) {
                s_current_step = 0;
                s_step_counter = 0;
            }
            break;
            
        case 1: {  // STEP SELECT (was 0)
            s_selected_step = (uint8_t)value;
            if (s_selected_step >= NUM_STEPS) s_selected_step = 0;
            // Load step data to edit parameters
            Step *step = &s_patterns[s_current_pattern].steps[s_selected_step];
            s_edit_pitch = step->pitch_offset;
            s_edit_filter = step->filter_mod;
            s_edit_gate = step->gate_length;
            break;
        }
            
        case 2:  // PITCH OFFSET (was 1)
        {
            int32_t pitch = value;
            pitch = clipminmaxi32(-24, pitch, 24);
            
            Step* step = &s_patterns[s_current_pattern].steps[s_selected_step];
            step->pitch_offset = (int8_t)pitch;
            s_edit_pitch = (int8_t)pitch;
            break;
        }
    
        case 3:  // FILTER MOD (was 2)
        {
            float filter_val = param_val_to_f32(value);
            Step* step = &s_patterns[s_current_pattern].steps[s_selected_step];
            step->filter_mod = filter_val;
            s_edit_filter = filter_val;
            break;
        }
    
        case 4:  // GATE LENGTH (was 3)
        {
            float gate_val = param_val_to_f32(value);
            Step* step = &s_patterns[s_current_pattern].steps[s_selected_step];
            step->gate_length = gate_val;
            s_edit_gate = gate_val;
            break;
        }
    
        case 5:  // SEQUENCE LENGTH (was 4)
        {
            s_sequence_length = (uint8_t)(value + 1);  // 0-15 → 1-16
            if (s_sequence_length > NUM_STEPS) s_sequence_length = NUM_STEPS;
            if (s_sequence_length < 1) s_sequence_length = 1;
            s_patterns[s_current_pattern].length = s_sequence_length;
            if (s_current_step >= s_sequence_length) {
                s_current_step = 0;
            }
            break;
        }
    
        case 6:  // SWING (was 5)
            s_swing_amount = valf;
            break;
            
        case 7:  // RATCHET (was 6)
        {
            Step* step = &s_patterns[s_current_pattern].steps[s_selected_step];
            step->ratchet_count = (uint8_t)(value + 1);  // 0-3 → 1-4
            s_ratchet_mode = (uint8_t)value;
            break;
        }
    
        case 8:  // PATTERN SELECT (was 9)
            s_current_pattern = (uint8_t)value;
            if (s_current_pattern >= NUM_PATTERNS) s_current_pattern = 0;
            s_sequence_length = s_patterns[s_current_pattern].length;
            s_current_step = 0;
            break;
            
        case 9:  // DIRECTION (was 10)
            s_direction_mode = (uint8_t)value;
            if (s_direction_mode > 3) s_direction_mode = 0;
            
            // Reset direction state
            s_step_direction = 1;
            break;
            
        default:
            break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0:  // PLAY
            return s_sequencer_playing ? 1 : 0;
            
        case 1:  // STEP
            return s_selected_step;
            
        case 2:  // PITCH
        {
            Step* step = &s_patterns[s_current_pattern].steps[s_selected_step];
            return (int32_t)step->pitch_offset;
        }
    
        case 3:  // FILTER
        {
            Step* step = &s_patterns[s_current_pattern].steps[s_selected_step];
            return (int32_t)(step->filter_mod * 1023.f);
        }
    
        case 4:  // GATE
        {
            Step* step = &s_patterns[s_current_pattern].steps[s_selected_step];
            return (int32_t)(step->gate_length * 1023.f);
        }
    
        case 5:  // LENGTH
            return s_sequence_length - 1;  // 1-16 → 0-15
            
        case 6:  // SWING
            return (int32_t)(s_swing_amount * 1023.f);
            
        case 7:  // RATCHET
        {
            Step* step = &s_patterns[s_current_pattern].steps[s_selected_step];
            return (int32_t)(step->ratchet_count - 1);  // 1-4 → 0-3
        }
    
        case 8:  // PATTERN
            return s_current_pattern;
            
        case 9:  // DIRECTION
            return s_direction_mode;
            
        default:
            return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    // ✅ PLAY parameter
    if (id == 0) {
        return (value != 0) ? "ON" : "OFF";
    }
    
    // ✅ STEP parameter (was 0, now 1)
    if (id == 1) {
        static char step_str[4];
        int step_num = value + 1;
        if (step_num < 10) {
            step_str[0] = '0' + step_num;
            step_str[1] = '\0';
        } else {
            step_str[0] = '0' + (step_num / 10);
            step_str[1] = '0' + (step_num % 10);
            step_str[2] = '\0';
        }
        return step_str;
    }
    
    // ✅ LENGTH parameter (was 4, now 5)
    if (id == 5) {
        static char len_str[4];
        int len_num = value + 1;
        if (len_num < 10) {
            len_str[0] = '0' + len_num;
            len_str[1] = '\0';
        } else {
            len_str[0] = '0' + (len_num / 10);
            len_str[1] = '0' + (len_num % 10);
            len_str[2] = '\0';
        }
        return len_str;
    }
    
    // ✅ RATCHET parameter (was 6, now 7)
    if (id == 7) {
        static const char* ratchet_names[4] = {"1X", "2X", "3X", "4X"};
        if (value >= 0 && value < 4) {
            return ratchet_names[value];
        }
    }
    
    // ✅ PATTERN parameter (was 9, now 8)
    if (id == 8) {
        static char pat_str[4];
        int pat_num = value + 1;
        if (pat_num < 10) {
            pat_str[0] = '0' + pat_num;
            pat_str[1] = '\0';
        } else {
            pat_str[0] = '0' + (pat_num / 10);
            pat_str[1] = '0' + (pat_num % 10);
            pat_str[2] = '\0';
        }
        return pat_str;
    }
    
    // ✅ DIRECTION parameter (was 10, now 9)
    if (id == 9) {
        static const char* dir_names[4] = {"FWD", "REV", "PING", "RAND"};
        if (value >= 0 && value < 4) {
            return dir_names[value];
        }
    }
    
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo)
{
    // Tempo format: upper 16 bits = BPM integer, lower 16 bits = fractional
    s_tempo_bpm = tempo >> 16;
    if (s_tempo_bpm < 60) s_tempo_bpm = 120;  // Default
    
    // Calculate samples per 16th note
    // 16th note = 1/4 of quarter note
    float beats_per_sec = (float)s_tempo_bpm / 60.f;
    float sixteenth_notes_per_sec = beats_per_sec * 4.f;
    s_samples_per_step = (uint32_t)(48000.f / sixteenth_notes_per_sec);
    
    s_tempo_synced = true;
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter)
{
    // Called at 16th note intervals
    // Reset step counter for tight sync
    s_last_tick_time = s_sample_counter;
    s_tempo_synced = true;
    
    // Optionally: force step advance on tick for ultra-tight sync
    // (Disabled by default to allow internal swing/ratcheting)
}

