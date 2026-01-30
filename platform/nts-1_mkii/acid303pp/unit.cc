/*
    BSD 3-Clause License

    Copyright (c) 2023, KORG INC.
    All rights reserved.

    ACID 303++ - Ultimate Acid Groove Machine
    NTS-1 mkII oscillator unit implementation
*/

#include "unit_osc.h"
#include "osc_api.h"
#include "utils/float_math.h"
#include "utils/int_math.h"

// SDK compatibility - PI is already defined in CMSIS arm_math.h
#ifndef PI
#define PI 3.14159265359f
#endif

// ========================================================================
// UNIVERSAL SEQUENCER MODULE - ADD TO ANY OSCILLATOR
// Based on J6 Oscillator by Tweeeeeak
// ========================================================================

// ========== SEQUENCER CONFIGURATION ==========

#define SEQ_STEPS 8   // ✅ Reduced from 16 to 8 to save memory
#define SEQ_SLOTS 2   // ✅ Reduced from 8 to 2 to save memory

// ========== SEQUENCER MODE ==========

enum SequencerMode {
    SEQ_OFF = 0,           // 0: Sequencer disabled
    SEQ_PLAY_STEP_1 = 1,   // 1-16: Play mode (shows current step)
    SEQ_PLAY_STEP_8 = 8,   // ✅ Reduced from 16
    SEQ_RECORD = 9         // ✅ Reduced from 17
};

// ========== SEQUENCER STEP ==========

struct SequencerStep {
    uint8_t note;          // MIDI note (0 = rest, 1-127 = note)
    uint8_t velocity;      // Note velocity (0-127, 0 = rest)
    // ✅ Removed is_rest bool - use note==0 instead to save memory
};

// ========== SEQUENCER SLOT ==========

struct SequencerSlot {
    SequencerStep steps[SEQ_STEPS];
    uint8_t step_length;   // How many times each step plays (1-8) ✅ Reduced
    uint8_t pattern_length; // Total steps in pattern (1-8) ✅ Reduced
};

// ========== SEQUENCER STATE ==========

struct Sequencer {
    SequencerSlot slots[SEQ_SLOTS];
    
    uint8_t current_slot;      // Active slot (0-3) ✅ Reduced from 0-7
    uint8_t current_step;       // Current playing step (0-15)
    uint8_t record_step;        // Current recording step (0-15)
    
    uint32_t step_counter;      // Sample counter for step timing
    uint32_t samples_per_step;  // Samples per step (tempo sync)
    uint8_t step_repeat_count;  // Current repeat count
    
    uint8_t mode;               // Sequencer mode (0-9) ✅ Reduced from 0-17
    
    bool running;               // Is sequencer playing?
    bool recording;             // Is sequencer recording?
    
    uint32_t note_hold_time;    // For rest detection (hold >1 sec)
    uint8_t last_note_pressed;  // For rest detection
    bool note_is_held;          // Is a note currently held?
};

static Sequencer s_seq;

// ========== SEQUENCER INITIALIZATION ==========

inline void sequencer_init() {
    // Clear all slots
    for (uint8_t slot = 0; slot < SEQ_SLOTS; slot++) {
        s_seq.slots[slot].step_length = 1;
        s_seq.slots[slot].pattern_length = SEQ_STEPS;  // ✅ Use SEQ_STEPS constant
        
        for (uint8_t step = 0; step < SEQ_STEPS; step++) {
            s_seq.slots[slot].steps[step].note = 0;  // 0 = rest
            s_seq.slots[slot].steps[step].velocity = 100;
        }
    }
    
    s_seq.current_slot = 0;
    s_seq.current_step = 0;
    s_seq.record_step = 0;
    s_seq.step_counter = 0;
    s_seq.samples_per_step = 12000;  // 120 BPM, 16th notes
    s_seq.step_repeat_count = 0;
    s_seq.mode = SEQ_OFF;
    s_seq.running = false;
    s_seq.recording = false;
    s_seq.note_hold_time = 0;
    s_seq.last_note_pressed = 0;
    s_seq.note_is_held = false;
}

// ========== SEQUENCER MODE CONTROL ==========

inline void sequencer_set_mode(uint8_t mode) {
    s_seq.mode = mode;
    
    if (mode == SEQ_OFF) {
        // Turn off sequencer
        s_seq.running = false;
        s_seq.recording = false;
        s_seq.current_step = 0;
        s_seq.record_step = 0;
        
    } else if (mode == SEQ_RECORD) {
        // Enter record mode
        s_seq.recording = true;
        s_seq.running = false;
        s_seq.record_step = 0;
        
        // Clear current slot
        for (uint8_t i = 0; i < SEQ_STEPS; i++) {
            s_seq.slots[s_seq.current_slot].steps[i].note = 0;  // 0 = rest
            s_seq.slots[s_seq.current_slot].steps[i].velocity = 0;
        }
        
    } else if (mode >= SEQ_PLAY_STEP_1 && mode <= SEQ_PLAY_STEP_8) {  // ✅ Reduced from SEQ_PLAY_STEP_16
        // Enter play mode at specific step
        s_seq.running = true;
        s_seq.recording = false;
        s_seq.current_step = mode - 1;  // Mode 1 = step 0
        s_seq.step_counter = 0;
        s_seq.step_repeat_count = 0;
    }
}

// ========== RECORD NOTE ==========

inline void sequencer_record_note(uint8_t note, uint8_t velocity, bool is_rest) {
    if (!s_seq.recording) return;
    
    SequencerSlot* slot = &s_seq.slots[s_seq.current_slot];
    
    // Record to current step
    // ✅ Use note==0 for rest instead of is_rest bool
    slot->steps[s_seq.record_step].note = is_rest ? 0 : note;
    slot->steps[s_seq.record_step].velocity = is_rest ? 0 : velocity;
    
    // Auto-advance to next step (if in full record mode)
    if (s_seq.mode == SEQ_RECORD) {
        s_seq.record_step++;
        
        // After 16 steps, auto-enter play mode
        if (s_seq.record_step >= SEQ_STEPS) {
            s_seq.recording = false;
            s_seq.running = true;
            s_seq.current_step = 0;
            s_seq.mode = SEQ_PLAY_STEP_1;
        }
    }
    // If in step record mode (1-16), stay on same step
}

// ========== SEQUENCER PLAYBACK ==========

inline bool sequencer_get_next_note(uint8_t* out_note, uint8_t* out_velocity) {
    if (!s_seq.running) return false;
    
    SequencerSlot* slot = &s_seq.slots[s_seq.current_slot];
    SequencerStep* step = &slot->steps[s_seq.current_step];
    
    // Check if step is a rest (note==0 means rest)
    if (step->note == 0) {
        return false;
    }
    
    // Return note
    *out_note = step->note;
    *out_velocity = step->velocity;
    return true;
}

// ========== SEQUENCER ADVANCE ==========

inline void sequencer_advance() {
    if (!s_seq.running) return;
    
    s_seq.step_counter++;
    
    // Check if we need to advance step
    if (s_seq.step_counter >= s_seq.samples_per_step) {
        s_seq.step_counter = 0;
        s_seq.step_repeat_count++;
        
        SequencerSlot* slot = &s_seq.slots[s_seq.current_slot];
        
        // Check if we need to advance to next step
        if (s_seq.step_repeat_count >= slot->step_length) {
            s_seq.step_repeat_count = 0;
            s_seq.current_step++;
            
            // Loop pattern
            if (s_seq.current_step >= slot->pattern_length) {
                s_seq.current_step = 0;
            }
            
            // Update mode display (1-16)
            s_seq.mode = s_seq.current_step + 1;
        }
    }
}

// ========== NOTE HOLD DETECTION (FOR RESTS) ==========

inline void sequencer_note_hold_check() {
    if (!s_seq.note_is_held) return;
    
    s_seq.note_hold_time++;
    
    // If held for more than 1 second (48000 samples), record as rest
    if (s_seq.note_hold_time > 48000) {
        sequencer_record_note(0, 0, true);  // Record rest
        s_seq.note_is_held = false;
        s_seq.note_hold_time = 0;
    }
}

// ========== INTEGRATION FUNCTIONS ==========

// Call this in unit_init()
inline void sequencer_unit_init() {
    sequencer_init();
}

// Call this at START of unit_render() - once per frame!
inline void sequencer_process_frame() {
    if (s_seq.running) {
        sequencer_advance();
    }
    
    if (s_seq.recording) {
        sequencer_note_hold_check();
    }
}

// Call this in unit_note_on()
inline bool sequencer_handle_note_on(uint8_t note, uint8_t velocity, 
                                     uint8_t* actual_note, uint8_t* actual_velocity) {
    // If recording
    if (s_seq.recording) {
        // Start hold timer
        s_seq.note_is_held = true;
        s_seq.note_hold_time = 0;
        s_seq.last_note_pressed = note;
        
        // Record note immediately
        sequencer_record_note(note, velocity, false);
        
        // Play the note normally
        *actual_note = note;
        *actual_velocity = velocity;
        return true;
    }
    
    // If playing
    if (s_seq.running) {
        // Get note from sequencer
        if (sequencer_get_next_note(actual_note, actual_velocity)) {
            return true;
        }
        // If current step is rest, don't play anything
        return false;
    }
    
    // If sequencer off, play normally
    *actual_note = note;
    *actual_velocity = velocity;
    return true;
}

// Call this in unit_note_off()
inline void sequencer_handle_note_off(uint8_t note) {
    // Stop hold timer
    if (s_seq.recording && note == s_seq.last_note_pressed) {
        s_seq.note_is_held = false;
        s_seq.note_hold_time = 0;
    }
}

// Call this in unit_set_param_value()
inline void sequencer_set_param(uint8_t param_id, int32_t value) {
    // Parameters 10, 11, 12 are sequencer params
    switch (param_id) {
        case 10:  // SEQMODE
            sequencer_set_mode((uint8_t)value);
            break;
            
        case 11:  // SEQSLOT
            s_seq.current_slot = (uint8_t)value;
            if (s_seq.current_slot >= SEQ_SLOTS) s_seq.current_slot = SEQ_SLOTS - 1;  // ✅ Clamp to max
            break;
            
        case 12:  // STPLEN
        {
            uint8_t step_length = (uint8_t)value;
            if (step_length < 1) step_length = 1;
            if (step_length > SEQ_STEPS) step_length = SEQ_STEPS;  // ✅ Use SEQ_STEPS constant
            s_seq.slots[s_seq.current_slot].step_length = step_length;
            break;
        }
    }
}

// Call this in unit_get_param_value()
inline int32_t sequencer_get_param(uint8_t param_id) {
    switch (param_id) {
        case 10:  // SEQMODE
            return s_seq.mode;
            
        case 11:  // SEQSLOT
            return s_seq.current_slot;
            
        case 12:  // STPLEN
            return s_seq.slots[s_seq.current_slot].step_length;
            
        default:
            return 0;
    }
}

// Call this in unit_get_param_str_value()
inline const char* sequencer_get_param_str(uint8_t param_id, int32_t value) {
    static char buf[8];
    
    if (param_id == 10) {  // SEQMODE
        if (value == 0) return "OFF";
        if (value == 9) return "REC";  // ✅ Reduced from 17
        // Play mode shows current step (1-8) ✅ Reduced from 1-16
        // Convert integer to string (0-8)
        buf[0] = '0' + (char)value;
        buf[1] = '\0';
        return buf;
    }
    
    if (param_id == 11) {  // SEQSLOT
        // Display 1-2 (slots 0-1)
        buf[0] = '0' + (char)(value + 1);
        buf[1] = '\0';
        return buf;
    }
    
    if (param_id == 12) {  // STPLEN
        // Display 1-8
        buf[0] = '0' + (char)value;
        buf[1] = '\0';
        return buf;
    }
    
    return "";
}

// Call this in unit_set_tempo()
inline void sequencer_set_tempo(uint32_t tempo) {
    float bpm = (float)(tempo >> 16) + (float)(tempo & 0xFFFF) / 65536.f;
    bpm = clipminmaxf(60.f, bpm, 240.f);
    
    // Calculate samples per step (16th notes)
    s_seq.samples_per_step = (uint32_t)((60.f / bpm) * 48000.f / 4.f);
    // Clamp to safe range
    if (s_seq.samples_per_step < 3000) s_seq.samples_per_step = 3000;
    if (s_seq.samples_per_step > 48000) s_seq.samples_per_step = 48000;
}

// ========================================================================
// END OF UNIVERSAL SEQUENCER MODULE
// ========================================================================

// Filter pole count
#define FILTER_POLES 4

// Envelope stages
enum EnvStage {
    ENV_OFF = 0,
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN
};

// Filter modes
enum FilterMode {
    FILT_SERIAL = 0,    // 4-pole cascade
    FILT_PARALLEL,      // 2× 2-pole (stereo)
    FILT_BANDPASS,      // 2-pole BP
    FILT_NOTCH          // Notch filter
};

// Waveform types
enum Waveform {
    WAVE_SQUARE = 0,
    WAVE_SAW,
    WAVE_TRIANGLE,
    WAVE_PULSE
};

// Distortion flavors
enum DistType {
    DIST_SOFT = 0,
    DIST_HARD,
    DIST_FOLD,
    DIST_BIT
};

// State structure
struct AcidState {
    // Oscillator
    float phase;
    float phase_sub1;  // -1 octave
    float phase_sub2;  // -2 octave
    float target_freq;
    float current_freq;
    
    // Note info
    uint8_t current_note;
    uint8_t velocity;
    bool note_active;
    bool slide_active;
    
    // Filter (4 pole cascade)
    float filt_z1[FILTER_POLES];
    float filt_z2[FILTER_POLES];
    float filt_cutoff;
    float filt_reso;
    
    // Envelopes
    EnvStage amp_env_stage;
    float amp_env_level;
    uint32_t amp_env_counter;
    
    EnvStage filt_env_stage;
    float filt_env_level;
    uint32_t filt_env_counter;
    
    // LFO (Sample & Hold)
    float lfo_phase;
    float lfo_value;
    float lfo_rate;
    
    // Envelope follower
    float env_follow_state;
    
    // Previous sample (for slide detection)
    uint32_t last_note_time;
};

static AcidState s_state;

// Parameters
static float s_cutoff = 0.6f;
static float s_slide_time = 0.35f;
static float s_resonance = 0.85f;
static float s_env_amount = 0.75f;
static Waveform s_waveform = WAVE_SAW;
static float s_amp_decay = 0.25f;
static float s_filt_decay = 0.4f;
static float s_distortion = 0.0f;  // -1.0 (pre) to +1.0 (post)
static FilterMode s_filter_mode = FILT_SERIAL;
static float s_sub_mix = 0.6f;

// Advanced features (niet in parameters, maar wel actief!)
static float s_filter_tracking = 0.5f;  // 🎁 SURPRISE!
static float s_accent_amount = 0.7f;    // 🎁 SURPRISE!
static DistType s_dist_flavor = DIST_SOFT;  // 🎁 SURPRISE!
static bool s_lfo_enabled = true;       // 🎁 SURPRISE!

// XORShift random
static uint32_t s_random_seed = 12345;

inline uint32_t xorshift32() {
    s_random_seed ^= s_random_seed << 13;
    s_random_seed ^= s_random_seed >> 17;
    s_random_seed ^= s_random_seed << 5;
    return s_random_seed;
}

inline float random_float() {
    return (float)(xorshift32() % 10000) / 10000.f;
}

// Simple tan approximation for filter (when osc_tanf not available)
inline float fast_tanf(float x) {
    // Polynomial approximation for tan(x) in [-PI/4, PI/4]
    float x2 = x * x;
    float x4 = x2 * x2;
    return x * (1.f + x2 * (0.333333f + x2 * (0.133333f + x2 * 0.053968f)));
}

// Moog ladder filter (4-pole cascade)
inline float moog_ladder_filter(float input, float cutoff, float resonance) {
    // ✅ SAFE cutoff range: 500Hz - 15.5kHz (not 20Hz!)
    float freq = 500.f + cutoff * 15000.f;
    freq = clipminmaxf(500.f, freq, 20000.f);
    
    // Resonance compensation (prevents volume drop)
    float reso_comp = 1.f + resonance * 0.5f;
    input *= reso_comp;
    
    // Calculate filter coefficient (simplified Moog ladder)
    float w = 2.f * PI * freq / 48000.f;
    w = clipminmaxf(0.001f, w, PI * 0.95f);  // Prevent instability
    
    // Simplified g calculation for Moog ladder
    float g = 0.9892f * w - 0.4342f * w * w + 0.1381f * w * w * w - 0.0202f * w * w * w * w;
    g = clipminmaxf(0.001f, g, 0.99f);
    
    // ✅ Feedback limited (prevent self-oscillation noise)
    float feedback = resonance * 3.5f;  // Max 3.5 (was 4.0)
    feedback = clipminmaxf(0.f, feedback, 3.5f);  // Prevent explosion
    
    // 4-pole cascade
    float x = input - s_state.filt_z2[3] * feedback;
    
    for (int pole = 0; pole < FILTER_POLES; pole++) {
        // One-pole lowpass
        s_state.filt_z1[pole] += g * (x - s_state.filt_z1[pole]);
        s_state.filt_z2[pole] = s_state.filt_z1[pole];
        
        // Clip to prevent explosion
        s_state.filt_z1[pole] = clipminmaxf(-2.f, s_state.filt_z1[pole], 2.f);
        s_state.filt_z2[pole] = clipminmaxf(-2.f, s_state.filt_z2[pole], 2.f);
        
        x = s_state.filt_z2[pole];
    }
    
    return s_state.filt_z2[3];  // Output from last pole
}

// Multi-mode filter system
inline float process_filter(float input, float cutoff, float resonance) {
    switch (s_filter_mode) {
        case FILT_SERIAL:
            // Classic 4-pole cascade
            return moog_ladder_filter(input, cutoff, resonance);
        
        case FILT_PARALLEL: {
            // 2× 2-pole filters (stereo width effect)
            float left = input;
            float right = input;
            
            // Filter 1 (slightly detuned)
            for (int i = 0; i < 2; i++) {
                float freq = 500.f + (cutoff * 0.98f) * 15000.f;  // ✅ 500Hz minimum
                freq = clipminmaxf(500.f, freq, 20000.f);
                float w = 2.f * PI * freq / 48000.f;
                w = clipminmaxf(0.001f, w, PI * 0.95f);
                float g = 0.9892f * w - 0.4342f * w * w;
                g = clipminmaxf(0.001f, g, 0.99f);
                s_state.filt_z1[i] += g * (left - s_state.filt_z1[i]);
                left = s_state.filt_z1[i];
                s_state.filt_z1[i] = clipminmaxf(-2.f, s_state.filt_z1[i], 2.f);
            }
            
            // Filter 2 (slightly detuned opposite)
            for (int i = 2; i < 4; i++) {
                float freq = 500.f + (cutoff * 1.02f) * 15000.f;  // ✅ 500Hz minimum
                freq = clipminmaxf(500.f, freq, 20000.f);
                float w = 2.f * PI * freq / 48000.f;
                w = clipminmaxf(0.001f, w, PI * 0.95f);
                float g = 0.9892f * w - 0.4342f * w * w;
                g = clipminmaxf(0.001f, g, 0.99f);
                s_state.filt_z1[i] += g * (right - s_state.filt_z1[i]);
                right = s_state.filt_z1[i];
                s_state.filt_z1[i] = clipminmaxf(-2.f, s_state.filt_z1[i], 2.f);
            }
            
            return (left + right) * 0.5f;
        }
        
        case FILT_BANDPASS: {
            // 2-pole bandpass (vocal)
            float freq = 500.f + cutoff * 15000.f;  // ✅ 500Hz minimum
            freq = clipminmaxf(500.f, freq, 20000.f);
            float w = 2.f * PI * freq / 48000.f;
            w = clipminmaxf(0.001f, w, PI * 0.95f);
            float f = 2.f * osc_sinf(w * 0.5f);
            float q = 1.f / (0.5f + resonance * 4.f);
            q = clipminmaxf(0.3f, q, 3.0f);
            
            s_state.filt_z2[0] += f * s_state.filt_z1[0];
            float hp = input - s_state.filt_z2[0] - q * s_state.filt_z1[0];
            s_state.filt_z1[0] += f * hp;
            
            // Clip states
            s_state.filt_z1[0] = clipminmaxf(-2.f, s_state.filt_z1[0], 2.f);
            s_state.filt_z2[0] = clipminmaxf(-2.f, s_state.filt_z2[0], 2.f);
            
            return s_state.filt_z1[0];  // Bandpass output
        }
        
        case FILT_NOTCH: {
            // Notch filter (hollow sound)
            float filtered = moog_ladder_filter(input, cutoff, resonance * 0.5f);
            return input - filtered;  // Phase cancellation
        }
        
        default:
            return input;
    }
}

// Distortion engine (4 flavors!)
inline float apply_distortion(float input, float amount, DistType type) {
    if (fabsf(amount) < 0.01f) return input;
    
    float gain = 1.f + fabsf(amount) * 9.f;  // 1-10× gain
    float x = input * gain;
    
    switch (type) {
        case DIST_SOFT: {
            // Tube-style soft clipping
            if (x < -1.5f) x = -1.f;
            else if (x > 1.5f) x = 1.f;
            else {
                float x2 = x * x;
                x = x * (27.f + x2) / (27.f + 9.f * x2);
            }
            break;
        }
        
        case DIST_HARD: {
            // Transistor hard clipping
            float threshold = 0.8f;
            if (x > threshold) x = threshold + (x - threshold) * 0.1f;
            if (x < -threshold) x = -threshold + (x + threshold) * 0.1f;
            x = clipminmaxf(-1.f, x, 1.f);
            break;
        }
        
        case DIST_FOLD: {
            // Wave folding (!)
            while (x > 1.f) x = 2.f - x;
            while (x < -1.f) x = -2.f - x;
            break;
        }
        
        case DIST_BIT: {
            // Bitcrusher
            int bits = 12 - (int)(fabsf(amount) * 8.f);  // 12-4 bits
            bits = clipminmaxi32(4, bits, 12);
            float steps = (float)(1 << bits);
            // Use integer casting instead of floorf
            x = (float)((int32_t)(x * steps + 0.5f)) / steps;
            break;
        }
    }
    
    return x / gain;  // Compensate gain
}

// Envelope generators
inline float process_amp_envelope() {
    s_state.amp_env_counter++;
    
    switch (s_state.amp_env_stage) {
        case ENV_ATTACK: {
            // Super fast attack (1ms)
            s_state.amp_env_level += 1.f / 48.f;
            if (s_state.amp_env_level >= 1.f) {
                s_state.amp_env_level = 1.f;
                s_state.amp_env_stage = ENV_DECAY;
                s_state.amp_env_counter = 0;
            }
            break;
        }
        
        case ENV_DECAY: {
            // Exponential decay
            float decay_samples = 48.f + s_amp_decay * 48000.f;  // 1ms - 1sec
            decay_samples = clipminmaxf(48.f, decay_samples, 48000.f);
            float decay_coeff = fastexpf(-4.f / decay_samples);
            s_state.amp_env_level *= decay_coeff;
            
            // ✅ Als envelope volledig weg
            if (s_state.amp_env_level < 0.001f) {
                s_state.amp_env_level = 0.f;
                s_state.amp_env_stage = ENV_OFF;
                s_state.note_active = false;  // ✅ NU pas note inactive!
            }
            break;
        }
        
        default:
            s_state.amp_env_level = 0.f;
            break;
    }
    
    // Velocity scaling
    float vel_scale = (float)s_state.velocity / 127.f;
    return s_state.amp_env_level * vel_scale;
}

inline float process_filter_envelope() {
    s_state.filt_env_counter++;
    
    switch (s_state.filt_env_stage) {
        case ENV_ATTACK: {
            // Instant attack
            s_state.filt_env_level = 1.f;
            s_state.filt_env_stage = ENV_DECAY;
            s_state.filt_env_counter = 0;
            break;
        }
        
        case ENV_DECAY: {
            // Filter decay
            float decay_samples = 100.f + s_filt_decay * 24000.f;  // 2ms - 500ms
            decay_samples = clipminmaxf(100.f, decay_samples, 24000.f);
            float decay_coeff = fastexpf(-4.f / decay_samples);
            s_state.filt_env_level *= decay_coeff;
            
            if (s_state.filt_env_level < 0.01f) {
                s_state.filt_env_level = 0.f;
                s_state.filt_env_stage = ENV_OFF;
            }
            break;
        }
        
        default:
            s_state.filt_env_level = 0.f;
            break;
    }
    
    return s_state.filt_env_level;
}

// Slide (portamento) system
inline void update_slide() {
    if (!s_state.slide_active) {
        s_state.current_freq = s_state.target_freq;
        return;
    }
    
    // Slide speed (10ms - 500ms)
    float slide_samples = 480.f + s_slide_time * 24000.f;
    slide_samples = clipminmaxf(480.f, slide_samples, 24000.f);
    float slide_coeff = 1.f - fastexpf(-1.f / slide_samples);
    
    // Smooth exponential slide
    s_state.current_freq += (s_state.target_freq - s_state.current_freq) * slide_coeff;
    
    // Snap when close enough
    if (fabsf(s_state.target_freq - s_state.current_freq) < 0.001f) {
        s_state.current_freq = s_state.target_freq;
        s_state.slide_active = false;
    }
}

// Sample & Hold LFO (surprise!)
inline void update_sample_hold_lfo() {
    if (!s_lfo_enabled) return;
    
    // LFO rate: 0.5Hz - 16Hz
    float lfo_hz = 0.5f + s_state.lfo_rate * 15.5f;
    float lfo_inc = lfo_hz / 48000.f;
    
    s_state.lfo_phase += lfo_inc;
    
    // Sample & Hold on phase wrap
    if (s_state.lfo_phase >= 1.f) {
        s_state.lfo_phase -= 1.f;
        s_state.lfo_value = random_float();  // New random value
    }
}

// Waveform generation
inline float generate_waveform(float phase, Waveform type) {
    switch (type) {
        case WAVE_SQUARE:
            return (phase < 0.5f) ? 1.f : -1.f;
        
        case WAVE_SAW:
            return 2.f * phase - 1.f;
        
        case WAVE_TRIANGLE: {
            float tri = (phase < 0.5f) ? (4.f * phase - 1.f) : (3.f - 4.f * phase);
            return tri;
        }
        
        case WAVE_PULSE: {
            // PWM: 10-90% duty cycle
            float duty = 0.1f + s_cutoff * 0.8f;  // Modulate with cutoff!
            return (phase < duty) ? 1.f : -1.f;
        }
        
        default:
            return 0.f;
    }
}

// Sub oscillator engine
inline float generate_sub_oscillators() {
    // -1 octave (square wave)
    float sub1 = (s_state.phase_sub1 < 0.5f) ? 1.f : -1.f;
    
    // -2 octave (sine wave, 808-style)
    float sub2 = osc_sinf(s_state.phase_sub2);
    
    // Mix: 70% sub1, 30% sub2
    return (sub1 * 0.7f + sub2 * 0.3f) * s_sub_mix;
}

// Accent system (surprise!)
inline float get_accent_boost() {
    // High velocity = accent
    float vel_norm = (float)s_state.velocity / 127.f;
    
    if (vel_norm > 0.8f) {
        // Accent mode: boost filter + resonance
        return 1.f + (vel_norm - 0.8f) * s_accent_amount * 2.f;  // Up to 40% boost
    }
    
    return 1.f;  // No accent
}

// Main render loop
__unit_callback void unit_render(const float * in, float * out, uint32_t frames) {
    (void)in;
    
    for (uint32_t f = 0; f < frames; f++) {
        // ✅ ADD: Process sequencer FIRST (once per frame)
        if (f == 0) {
            sequencer_process_frame();
        }
        if (!s_state.note_active) {
            out[f] = 0.f;
            continue;
        }
        
        // Update slide
        update_slide();
        
        // Update S&H LFO
        update_sample_hold_lfo();
        
        // Generate main oscillator
        float osc_main = generate_waveform(s_state.phase, s_waveform);
        
        // Add sub oscillators
        float osc_sub = generate_sub_oscillators();
        float osc_out = osc_main + osc_sub;
        
        // Pre-filter distortion?
        if (s_distortion < 0.f) {
            osc_out = apply_distortion(osc_out, -s_distortion, s_dist_flavor);
        }
        
        // Calculate filter cutoff with modulation
        float base_cutoff = s_cutoff;
        
        // Filter envelope modulation
        float filt_env = process_filter_envelope();
        base_cutoff += filt_env * s_env_amount;
        
        // LFO modulation (surprise!)
        if (s_lfo_enabled) {
            base_cutoff += (s_state.lfo_value - 0.5f) * 0.3f;  // ±15%
        }
        
        // Keyboard tracking (surprise!)
        float note_norm = (float)(s_state.current_note - 36) / 48.f;  // C2 = center
        base_cutoff += note_norm * s_filter_tracking * 0.5f;
        
        // Accent boost (surprise!)
        float accent = get_accent_boost();
        base_cutoff *= accent;
        
        base_cutoff = clipminmaxf(0.f, base_cutoff, 1.f);
        
        // Apply filter
        float filtered = process_filter(osc_out, base_cutoff, s_resonance * accent);
        
        // Post-filter distortion?
        if (s_distortion > 0.f) {
            filtered = apply_distortion(filtered, s_distortion, s_dist_flavor);
        }
        
        // Apply amp envelope
        float amp_env = process_amp_envelope();
        float final_out = filtered * amp_env;
        
        // Output with ACID BOOST GAIN!
        final_out *= 3.5f;  // 🔥 LOUD ACID BASS!
        
        out[f] = clipminmaxf(-1.f, final_out, 1.f);
        
        // Advance oscillator phases
        s_state.phase += s_state.current_freq;
        if (s_state.phase >= 1.f) s_state.phase -= 1.f;
        
        s_state.phase_sub1 += s_state.current_freq * 0.5f;  // -1 octave
        if (s_state.phase_sub1 >= 1.f) s_state.phase_sub1 -= 1.f;
        
        s_state.phase_sub2 += s_state.current_freq * 0.25f;  // -2 octaves
        if (s_state.phase_sub2 >= 1.f) s_state.phase_sub2 -= 1.f;
    }
}

// Note callbacks
__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
    uint8_t actual_note;
    uint8_t actual_velocity;
    
    // ✅ ADD: Let sequencer handle note
    if (!sequencer_handle_note_on(note, velocity, &actual_note, &actual_velocity)) {
        return;  // Rest step, don't play
    }
    
    // Use actual_note and actual_velocity instead of note/velocity
    uint32_t current_time = s_state.amp_env_counter;  // Use as timestamp
    uint32_t time_since_last = current_time - s_state.last_note_time;
    
    // Auto-detect slide: notes within 100ms = slide
    bool overlap = (time_since_last < 4800);  // 100ms @ 48kHz
    
    // Slide if: 1) overlap detected, OR 2) slide param > 0
    if ((overlap || s_slide_time > 0.01f) && s_state.note_active) {
        // SLIDE to new note
        s_state.slide_active = true;
        s_state.target_freq = osc_w0f_for_note(actual_note, 0);
        // Don't retrigger envelopes!
    } else {
        // NEW NOTE (no slide)
        s_state.slide_active = false;
        s_state.current_freq = osc_w0f_for_note(actual_note, 0);
        s_state.target_freq = s_state.current_freq;
        
        // Reset phases
        s_state.phase = 0.f;
        s_state.phase_sub1 = 0.f;
        s_state.phase_sub2 = 0.f;
        
        // Trigger envelopes
        s_state.amp_env_stage = ENV_ATTACK;
        s_state.amp_env_level = 0.f;
        s_state.amp_env_counter = 0;
        
        s_state.filt_env_stage = ENV_ATTACK;
        s_state.filt_env_level = 0.f;
        s_state.filt_env_counter = 0;
        
        // Reset filter states (prevent clicks)
        for (int i = 0; i < FILTER_POLES; i++) {
            s_state.filt_z1[i] = 0.f;
            s_state.filt_z2[i] = 0.f;
        }
    }
    
    s_state.current_note = actual_note;
    s_state.velocity = actual_velocity;
    s_state.note_active = true;
    s_state.last_note_time = current_time;
}

__unit_callback void unit_note_off(uint8_t note) {
    // ✅ ADD: Notify sequencer
    sequencer_handle_note_off(note);
    
    if (s_state.current_note == note) {
        // Let envelope finish naturally (no gate)
        // Acid bass = always decay, no sustain
    }
}

__unit_callback void unit_all_note_off() {
    s_state.note_active = false;
    s_state.amp_env_stage = ENV_OFF;
    s_state.filt_env_stage = ENV_OFF;
}

// Parameter handling
__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_cutoff = valf; break;
        case 1: s_slide_time = valf; break;
        case 2: s_resonance = valf; break;
        case 3: s_env_amount = valf; break;
        case 4: s_waveform = (Waveform)value; break;
        case 5: s_amp_decay = valf; break;
        case 6: s_filt_decay = valf; break;
        case 7:
            // Bipolar: -1023 to +1023
            s_distortion = (float)value / 1023.f;
            break;
        case 8: s_filter_mode = (FilterMode)value; break;
        case 9: s_sub_mix = valf; break;
        
        // ✅ Sequencer parameters removed (was 10, 11, 12) to fix payload size
        
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)(s_cutoff * 1023.f);
        case 1: return (int32_t)(s_slide_time * 1023.f);
        case 2: return (int32_t)(s_resonance * 1023.f);
        case 3: return (int32_t)(s_env_amount * 1023.f);
        case 4: return (int32_t)s_waveform;
        case 5: return (int32_t)(s_amp_decay * 1023.f);
        case 6: return (int32_t)(s_filt_decay * 1023.f);
        case 7: return (int32_t)(s_distortion * 1023.f);
        case 8: return (int32_t)s_filter_mode;
        case 9: return (int32_t)(s_sub_mix * 1023.f);
        
        // ✅ Sequencer parameters removed (was 10, 11, 12) to fix payload size
        
        default: return 0;
    }
}

__unit_callback const char* unit_get_param_str_value(uint8_t id, int32_t value) {
    // ✅ Sequencer parameters removed (was 10, 11, 12) to fix payload size
    
    if (id == 4) {  // Waveform
        static const char* waves[] = {"SQR", "SAW", "TRI", "PLS"};
        if (value >= 0 && value < 4) return waves[value];
    }
    if (id == 8) {  // Filter mode
        static const char* modes[] = {"4PLE", "PAR", "BP", "NOTCH"};
        if (value >= 0 && value < 4) return modes[value];
    }
    return "";
}

// Init & other callbacks
__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    
    // Init state
    s_state.phase = 0.f;
    s_state.phase_sub1 = 0.f;
    s_state.phase_sub2 = 0.f;
    s_state.current_freq = 0.f;
    s_state.target_freq = 0.f;
    s_state.current_note = 60;
    s_state.velocity = 100;
    s_state.note_active = false;
    s_state.slide_active = false;
    
    for (int i = 0; i < FILTER_POLES; i++) {
        s_state.filt_z1[i] = 0.f;
        s_state.filt_z2[i] = 0.f;
    }
    
    s_state.amp_env_stage = ENV_OFF;
    s_state.amp_env_level = 0.f;
    s_state.filt_env_stage = ENV_OFF;
    s_state.filt_env_level = 0.f;
    
    s_state.lfo_phase = 0.f;
    s_state.lfo_value = 0.5f;
    s_state.lfo_rate = 4.f / 16.f;  // ~4Hz default
    
    s_state.env_follow_state = 0.f;
    s_state.last_note_time = 0;
    
    // Init parameters with ACID defaults
    s_cutoff = 0.75f;  // ✅ 75% (was 0.6f - TE LAAG!)
    s_slide_time = 0.35f;
    s_resonance = 0.85f;
    s_env_amount = 0.75f;
    s_waveform = WAVE_SAW;
    s_amp_decay = 0.25f;
    s_filt_decay = 0.4f;
    s_distortion = 0.0f;
    s_filter_mode = FILT_SERIAL;
    s_sub_mix = 0.6f;
    
    // ✅ ADD: Initialize sequencer
    sequencer_unit_init();
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    // ✅ Complete reset
    s_state.note_active = false;
    s_state.slide_active = false;
    s_state.amp_env_stage = ENV_OFF;
    s_state.amp_env_level = 0.f;
    s_state.filt_env_stage = ENV_OFF;
    s_state.filt_env_level = 0.f;
    
    s_state.phase = 0.f;
    s_state.phase_sub1 = 0.f;
    s_state.phase_sub2 = 0.f;
    
    for (int i = 0; i < FILTER_POLES; i++) {
        s_state.filt_z1[i] = 0.f;
        s_state.filt_z2[i] = 0.f;
    }
}

__unit_callback void unit_resume() {
    // ✅ KRITIEK: Reset note state bij resume! (wordt aangeroepen bij oscillator switch)
    s_state.note_active = false;
    s_state.slide_active = false;
    s_state.amp_env_stage = ENV_OFF;
    s_state.amp_env_level = 0.f;
    s_state.filt_env_stage = ENV_OFF;
    s_state.filt_env_level = 0.f;
    
    // Reset phases
    s_state.phase = 0.f;
    s_state.phase_sub1 = 0.f;
    s_state.phase_sub2 = 0.f;
    
    // Reset filters
    for (int i = 0; i < FILTER_POLES; i++) {
        s_state.filt_z1[i] = 0.f;
        s_state.filt_z2[i] = 0.f;
    }
}
__unit_callback void unit_suspend() {}

// ✅ ADD: Tempo callback for sequencer
__unit_callback void unit_set_tempo(uint32_t tempo) {
    sequencer_set_tempo(tempo);
}

