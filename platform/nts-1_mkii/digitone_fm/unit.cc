/*
    DIGITONE FM - 4-Operator FM Synthesizer
    
    Authentic FM synthesis for NTS-1 mkII
*/

#include "unit_osc.h"
#include "osc_api.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "fx_api.h"

// ✅ FIX: Custom fast_tanh implementation (fastertanh2f doesn't exist)
inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// ========== SEQUENCER STRUCTURE ==========

#define SEQ_STEPS 16

enum SequencerMode {
    SEQ_OFF = 0,      // Sequencer disabled
    SEQ_PLAY = 1,     // Play recorded sequence
    SEQ_RECORD = 2    // Record notes in real-time
};

struct SequencerStep {
    uint8_t note;         // MIDI note (0 = rest)
    uint8_t velocity;     // Velocity
    bool active;          // Step active?
};

struct Sequencer {
    SequencerStep steps[SEQ_STEPS];
    uint8_t current_step;
    uint8_t length;          // Sequence length (1-16)
    uint32_t step_counter;   // Sample counter
    uint32_t samples_per_step;
    bool running;
    uint8_t last_played_note; // For note_off
};

static Sequencer s_seq;
static bool s_seq_playing = false;    // ✅ PLAY/STOP button
static bool s_seq_recording = false;  // ✅ REC mode (via note_on in REC)
static uint8_t s_seq_step_edit = 0;  // Which step to edit
static uint8_t s_root_note = 60;     // C4 default

// ========== FM OPERATOR STRUCTURE ==========

struct Operator {
    float phase;           // Oscillator phase
    float freq_ratio;      // Frequency multiplier
    float output;          // Current output
    float amp_env;         // Amplitude envelope
    float env_target;      // Envelope target
    bool active;
};

// ========== VOICE STRUCTURE ==========

struct Voice {
    Operator operators[4];  // 4 FM operators
    float base_freq;        // Base frequency (w0)
    float feedback_state;   // Feedback delay
    float filter_z1;        // Filter state 1
    float filter_z2;        // Filter state 2
    float lfo_phase;        // LFO phase
    uint32_t note_on_time;  // For envelope timing
    bool active;
};

static Voice s_voice;

// ========== PARAMETERS ==========

static uint8_t s_algorithm = 0;      // 0-7
static float s_fm_amount = 0.6f;     // FM depth
static float s_freq_ratio = 2.0f;    // Frequency ratio
static float s_feedback = 0.2f;      // Op1 feedback
static float s_attack = 0.05f;       // Envelope attack
static float s_decay = 0.5f;         // Envelope decay
static float s_filter_cutoff = 0.8f; // Filter cutoff
static float s_filter_resonance = 0.2f; // Filter Q

// ========== FM ALGORITHMS ==========

const char* algo_names[8] = {
    "1→2→3→4",  // Serial
    "1→2→3,4",  // Parallel carrier
    "1→2,3→4",  // Dual stacks
    "1→2→3,4",  // Mixed
    "1→234",    // One modulator
    "1→23,4",   // Asymmetric
    "1,2,3,4",  // All parallel
    "123→4"     // Triple mod
};

// Operator frequency ratios (musical intervals)
const float operator_ratios[4] = {
    1.0f,   // Op1: Base frequency
    2.0f,   // Op2: Octave up
    3.0f,   // Op3: Fifth above octave
    4.0f    // Op4: Two octaves up
};

// ========== SAFE PHASE WRAP ==========

inline float wrap_phase(float phase) {
    // ✅ CRITICAL: Proper phase wrapping
    while (phase >= 1.f) phase -= 1.f;
    while (phase < 0.f) phase += 1.f;
    return phase;
}

// ========== ENVELOPE GENERATOR (SAFE) ==========

inline void process_envelope(Operator* op, float attack_time, float decay_time) {
    // ✅ FIX: Safe envelope without clicks
    if (op->env_target > 0.5f) {
        // Attack phase
        float attack_rate = 0.001f + attack_time * 0.01f;  // ✅ Slower, safer
        attack_rate = clipminmaxf(0.001f, attack_rate, 0.1f);
        
        op->amp_env += attack_rate;
        
        if (op->amp_env >= 1.f) {
            op->amp_env = 1.f;
            op->env_target = 0.f;  // Switch to decay
        }
    } else {
        // Decay phase - exponential
        float decay_coeff = 0.9999f - (decay_time * 0.0005f);  // ✅ Gentler
        decay_coeff = clipminmaxf(0.995f, decay_coeff, 0.9999f);
        
        op->amp_env *= decay_coeff;
        
        if (op->amp_env < 0.001f) {
            op->amp_env = 0.f;
            op->active = false;
        }
    }
    
    // ✅ Safety clamp
    op->amp_env = clipminmaxf(0.f, op->amp_env, 1.f);
}

// ========== FM OPERATOR ==========

inline float process_operator(Operator* op, float modulation) {
    if (!op->active && op->amp_env < 0.001f) return 0.f;
    
    // ✅ FIX 1: SAFE FM DEPTH (max 0.5× instead of 5×)
    float fm_depth = s_fm_amount * 0.3f;  // Scale to 0-0.3 range
    fm_depth = clipminmaxf(0.f, fm_depth, 0.5f);  // Hard limit
    
    // ✅ FIX 2: Limit modulation input
    modulation = clipminmaxf(-2.f, modulation, 2.f);
    
    // ✅ FIX 3: Apply modulation with safety
    float mod_phase = op->phase + (modulation * fm_depth);
    
    // ✅ FIX 4: CRITICAL - Wrap phase properly!
    mod_phase = wrap_phase(mod_phase);
    
    // Generate sine wave
    float output = osc_sinf(mod_phase);
    
    // ✅ FIX: Soft limiting on output (using fast_tanh)
    output = fast_tanh(output * 0.9f);
    
    // Apply envelope
    output *= op->amp_env;
    
    // Store output
    op->output = output;
    
    // Advance phase
    float freq = s_voice.base_freq * op->freq_ratio;
    freq = clipminmaxf(0.0001f, freq, 0.45f);
    
    op->phase += freq;
    op->phase = wrap_phase(op->phase);  // ✅ Always wrap!
    
    return output;
}

// ========== FM ALGORITHM PROCESSOR ==========

inline float process_algorithm() {
    float output = 0.f;
    
    // Get operators
    Operator* op1 = &s_voice.operators[0];
    Operator* op2 = &s_voice.operators[1];
    Operator* op3 = &s_voice.operators[2];
    Operator* op4 = &s_voice.operators[3];
    
    // Process envelopes
    for (int i = 0; i < 4; i++) {
        process_envelope(&s_voice.operators[i], s_attack, s_decay);
    }
    
    // Algorithm routing
    switch (s_algorithm) {
        case 0: // 1→2→3→4 (Serial cascade)
        {
            // ✅ FIX: Safe feedback limiting
            float feedback_amount = s_feedback * 0.7f;  // Scale down
            feedback_amount = clipminmaxf(0.f, feedback_amount, 0.7f);
            
            // ✅ FIX: Limit feedback state
            s_voice.feedback_state = clipminmaxf(-1.f, s_voice.feedback_state, 1.f);
            float fb = s_voice.feedback_state * feedback_amount;
            
            float out1 = process_operator(op1, fb);
            float out2 = process_operator(op2, out1);
            float out3 = process_operator(op3, out2);
            float out4 = process_operator(op4, out3);
            
            // ✅ FIX: Smooth feedback update
            s_voice.feedback_state = s_voice.feedback_state * 0.5f + out1 * 0.5f;
            
            output = out4;
            break;
        }
        
        case 1: // 1→2→3, 1→4 (Parallel carriers)
        {
            float feedback_amount = s_feedback * 0.7f;
            feedback_amount = clipminmaxf(0.f, feedback_amount, 0.7f);
            s_voice.feedback_state = clipminmaxf(-1.f, s_voice.feedback_state, 1.f);
            float fb = s_voice.feedback_state * feedback_amount;
            
            float out1 = process_operator(op1, fb);
            float out2 = process_operator(op2, out1);
            float out3 = process_operator(op3, out2);
            float out4 = process_operator(op4, out1);
            
            s_voice.feedback_state = s_voice.feedback_state * 0.5f + out1 * 0.5f;
            output = (out3 + out4) * 0.5f;
            break;
        }
        
        case 2: // 1→2, 3→4 (Dual stacks)
        {
            float feedback_amount = s_feedback * 0.7f;
            feedback_amount = clipminmaxf(0.f, feedback_amount, 0.7f);
            s_voice.feedback_state = clipminmaxf(-1.f, s_voice.feedback_state, 1.f);
            float fb = s_voice.feedback_state * feedback_amount;
            
            float out1 = process_operator(op1, fb);
            float out2 = process_operator(op2, out1);
            float out3 = process_operator(op3, 0.f);
            float out4 = process_operator(op4, out3);
            
            s_voice.feedback_state = s_voice.feedback_state * 0.5f + out1 * 0.5f;
            output = (out2 + out4) * 0.5f;
            break;
        }
        
        case 3: // 1→2→3, 4 (Mixed)
        {
            float feedback_amount = s_feedback * 0.7f;
            feedback_amount = clipminmaxf(0.f, feedback_amount, 0.7f);
            s_voice.feedback_state = clipminmaxf(-1.f, s_voice.feedback_state, 1.f);
            float fb = s_voice.feedback_state * feedback_amount;
            
            float out1 = process_operator(op1, fb);
            float out2 = process_operator(op2, out1);
            float out3 = process_operator(op3, out2);
            float out4 = process_operator(op4, 0.f);
            
            s_voice.feedback_state = s_voice.feedback_state * 0.5f + out1 * 0.5f;
            output = (out3 + out4) * 0.5f;
            break;
        }
        
        case 4: // 1→2, 1→3, 1→4 (One modulator)
        {
            float feedback_amount = s_feedback * 0.7f;
            feedback_amount = clipminmaxf(0.f, feedback_amount, 0.7f);
            s_voice.feedback_state = clipminmaxf(-1.f, s_voice.feedback_state, 1.f);
            float fb = s_voice.feedback_state * feedback_amount;
            
            float out1 = process_operator(op1, fb);
            float out2 = process_operator(op2, out1);
            float out3 = process_operator(op3, out1);
            float out4 = process_operator(op4, out1);
            
            s_voice.feedback_state = s_voice.feedback_state * 0.5f + out1 * 0.5f;
            output = (out2 + out3 + out4) * 0.33f;
            break;
        }
        
        case 5: // 1→2, 1→3, 4 (Asymmetric)
        {
            float feedback_amount = s_feedback * 0.7f;
            feedback_amount = clipminmaxf(0.f, feedback_amount, 0.7f);
            s_voice.feedback_state = clipminmaxf(-1.f, s_voice.feedback_state, 1.f);
            float fb = s_voice.feedback_state * feedback_amount;
            
            float out1 = process_operator(op1, fb);
            float out2 = process_operator(op2, out1);
            float out3 = process_operator(op3, out1);
            float out4 = process_operator(op4, 0.f);
            
            s_voice.feedback_state = s_voice.feedback_state * 0.5f + out1 * 0.5f;
            output = (out2 + out3 + out4) * 0.33f;
            break;
        }
        
        case 6: // 1, 2, 3, 4 (All parallel - additive)
        {
            float feedback_amount = s_feedback * 0.7f;
            feedback_amount = clipminmaxf(0.f, feedback_amount, 0.7f);
            s_voice.feedback_state = clipminmaxf(-1.f, s_voice.feedback_state, 1.f);
            float fb = s_voice.feedback_state * feedback_amount;
            
            float out1 = process_operator(op1, fb);
            float out2 = process_operator(op2, 0.f);
            float out3 = process_operator(op3, 0.f);
            float out4 = process_operator(op4, 0.f);
            
            s_voice.feedback_state = s_voice.feedback_state * 0.5f + out1 * 0.5f;
            output = (out1 + out2 + out3 + out4) * 0.25f;
            break;
        }
        
        case 7: // 1→4, 2→4, 3→4 (Triple modulation)
        {
            float feedback_amount = s_feedback * 0.7f;
            feedback_amount = clipminmaxf(0.f, feedback_amount, 0.7f);
            s_voice.feedback_state = clipminmaxf(-1.f, s_voice.feedback_state, 1.f);
            float fb = s_voice.feedback_state * feedback_amount;
            
            float out1 = process_operator(op1, fb);
            float out2 = process_operator(op2, 0.f);
            float out3 = process_operator(op3, 0.f);
            
            float combined_mod = (out1 + out2 + out3) * 0.33f;
            float out4 = process_operator(op4, combined_mod);
            
            s_voice.feedback_state = s_voice.feedback_state * 0.5f + out1 * 0.5f;
            output = out4;
            break;
        }
    }
    
    // ✅ FIX: Final safety limiting
    output = clipminmaxf(-1.f, output, 1.f);
    
    return output;
}

// ========== STATE VARIABLE FILTER ==========

inline float process_filter(float input) {
    // ✅ FIX: Safe cutoff range (12kHz max for stability)
    float cutoff_hz = 100.f + s_filter_cutoff * 11900.f;  // 100Hz-12kHz (safe!)
    cutoff_hz = clipminmaxf(100.f, cutoff_hz, 12000.f);
    
    float w = 2.f * 3.14159265f * cutoff_hz / 48000.f;
    float f = 2.f * osc_sinf(w * 0.5f / (2.f * 3.14159265f));
    f = clipminmaxf(0.0001f, f, 1.5f);  // ✅ Lower max
    
    // ✅ FIX: Safe Q (lower range)
    float q = 1.f / (0.5f + s_filter_resonance * 1.0f);  // ✅ Lower range
    q = clipminmaxf(0.5f, q, 2.0f);  // ✅ Lower max
    
    // SVF processing
    s_voice.filter_z2 += f * s_voice.filter_z1;
    float hp = input - s_voice.filter_z2 - q * s_voice.filter_z1;
    s_voice.filter_z1 += f * hp;
    
    // Denormal kill
    if (si_fabsf(s_voice.filter_z1) < 1e-15f) s_voice.filter_z1 = 0.f;
    if (si_fabsf(s_voice.filter_z2) < 1e-15f) s_voice.filter_z2 = 0.f;
    
    // ✅ Hard clip states (tighter limits)
    s_voice.filter_z1 = clipminmaxf(-1.5f, s_voice.filter_z1, 1.5f);
    s_voice.filter_z2 = clipminmaxf(-1.5f, s_voice.filter_z2, 1.5f);
    
    return s_voice.filter_z2;  // Lowpass
}

// ========== MAIN OSCILLATOR ==========

inline float generate_oscillator() {
    if (!s_voice.active) return 0.f;
    
    // Check if any operator is active
    bool any_active = false;
    for (int i = 0; i < 4; i++) {
        if (s_voice.operators[i].active || s_voice.operators[i].amp_env > 0.001f) {
            any_active = true;
            break;
        }
    }
    
    if (!any_active) {
        s_voice.active = false;
        return 0.f;
    }
    
    // Process FM algorithm
    float fm_out = process_algorithm();
    
    // Apply filter
    fm_out = process_filter(fm_out);
    
    return fm_out;
}

// ========== SEQUENCER PROCESSOR ==========

inline void process_sequencer() {
    if (!s_seq_playing || !s_seq.running) return;
    
    // Advance step counter
    s_seq.step_counter++;
    
    // Check if we need to advance to next step
    if (s_seq.step_counter >= s_seq.samples_per_step) {
        s_seq.step_counter = 0;
        
        // Stop previous note
        if (s_seq.last_played_note > 0) {
            // Trigger note off internally
            s_voice.active = false;
        }
        
        // Get current step
        SequencerStep* step = &s_seq.steps[s_seq.current_step];
        
        // Trigger note if step is active
        if (step->active && step->note > 0) {
            // Trigger note on internally
            uint8_t note = step->note;
            
            // Reset all operator phases
            for (int i = 0; i < 4; i++) {
                s_voice.operators[i].phase = 0.f;
                s_voice.operators[i].amp_env = 0.f;
                s_voice.operators[i].env_target = 1.f;
                s_voice.operators[i].active = true;
                s_voice.operators[i].freq_ratio = operator_ratios[i] * 
                                                  (0.5f + s_freq_ratio * 0.5f);
            }
            
            s_voice.feedback_state = 0.f;
            s_voice.filter_z1 = 0.f;
            s_voice.filter_z2 = 0.f;
            s_voice.base_freq = osc_w0f_for_note(note, 0);
            s_voice.active = true;
            s_voice.note_on_time = 0;
            
            s_seq.last_played_note = note;
        }
        
        // Advance to next step
        s_seq.current_step++;
        if (s_seq.current_step >= s_seq.length) {
            s_seq.current_step = 0;
        }
    }
}

// ========== UNIT CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    
    // Init voice
    s_voice.active = false;
    s_voice.base_freq = 0.f;
    s_voice.feedback_state = 0.f;
    s_voice.filter_z1 = 0.f;
    s_voice.filter_z2 = 0.f;
    s_voice.lfo_phase = 0.f;
    s_voice.note_on_time = 0;
    
    // Init operators
    for (int i = 0; i < 4; i++) {
        s_voice.operators[i].phase = 0.f;
        s_voice.operators[i].freq_ratio = operator_ratios[i];
        s_voice.operators[i].output = 0.f;
        s_voice.operators[i].amp_env = 0.f;
        s_voice.operators[i].env_target = 0.f;
        s_voice.operators[i].active = false;
    }
    
    // Init parameters
    s_algorithm = 0;
    s_fm_amount = 0.3f;  // ✅ FIX: 30% default (safer for clean sound)
    s_freq_ratio = 2.0f;
    s_feedback = 0.2f;
    s_attack = 0.05f;
    s_decay = 0.5f;
    s_filter_cutoff = 0.8f;
    s_filter_resonance = 0.2f;
    
    // Init sequencer
    s_seq.current_step = 0;
    s_seq.length = 16;
    s_seq.step_counter = 0;
    s_seq.samples_per_step = 12000;  // 120 BPM, 16th notes @ 48kHz
    s_seq.running = false;
    s_seq.last_played_note = 0;
    
    for (int i = 0; i < SEQ_STEPS; i++) {
        s_seq.steps[i].note = 0;      // Rest
        s_seq.steps[i].velocity = 100;
        s_seq.steps[i].active = false;
    }
    
    // Default pattern: C major scale
    s_seq.steps[0].note = 60;  s_seq.steps[0].active = true;   // C
    s_seq.steps[1].note = 62;  s_seq.steps[1].active = true;   // D
    s_seq.steps[2].note = 64;  s_seq.steps[2].active = true;   // E
    s_seq.steps[3].note = 65;  s_seq.steps[3].active = true;   // F
    s_seq.steps[4].note = 67;  s_seq.steps[4].active = true;   // G
    s_seq.steps[5].note = 69;  s_seq.steps[5].active = true;   // A
    s_seq.steps[6].note = 71;  s_seq.steps[6].active = true;   // B
    s_seq.steps[7].note = 72;  s_seq.steps[7].active = true;   // C
    
    s_seq_playing = false;
    s_seq_recording = false;
    s_seq_step_edit = 0;
    s_root_note = 60;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    s_voice.active = false;
    s_voice.filter_z1 = 0.f;
    s_voice.filter_z2 = 0.f;
    s_voice.feedback_state = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    (void)in;
    
    for (uint32_t f = 0; f < frames; f++) {
        // ✅ Process sequencer FIRST
        process_sequencer();
        
        // Generate FM oscillator
        float sample = generate_oscillator();
        
        // ✅ FIX: Increased output gain (was 1.5f, now 2.5f)
        sample *= 2.5f;
        
        // ✅ FIX: Soft limiting (using fast_tanh)
        sample = fast_tanh(sample * 0.7f) * 1.4f;
        
        // Hard limit
        sample = clipminmaxf(-1.f, sample, 1.f);
        
        // Mono output
        out[f] = sample;
        
        // Increment note time
        if (s_voice.active) {
            s_voice.note_on_time++;
        }
    }
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
    s_root_note = note;
    
    // ✅ RECORD MODE: Auto-activate when notes are played while PLAY is OFF
    if (!s_seq_playing) {
        // Automatically enter REC mode when playing notes
        s_seq_recording = true;
        
        s_seq.steps[s_seq_step_edit].note = note;
        s_seq.steps[s_seq_step_edit].velocity = velocity;
        s_seq.steps[s_seq_step_edit].active = true;
        
        s_seq_step_edit++;
        if (s_seq_step_edit >= SEQ_STEPS) {
            s_seq_step_edit = 0;
            s_seq_recording = false;  // Stop recording after full sequence
        }
    }
    
    // ✅ PLAY MODE: Sequencer handles notes, don't trigger voice
    if (s_seq_playing) {
        return;  // Sequencer handles notes
    }
    
    // ✅ OFF MODE: Normal operation
    // ✅ FIX: Reset ALL state on note_on
    for (int i = 0; i < 4; i++) {
        s_voice.operators[i].phase = 0.f;
        s_voice.operators[i].amp_env = 0.f;
        s_voice.operators[i].env_target = 1.f;
        s_voice.operators[i].active = true;
        s_voice.operators[i].output = 0.f;
        
        s_voice.operators[i].freq_ratio = operator_ratios[i] * 
                                          (0.5f + s_freq_ratio * 0.5f);
    }
    
    // ✅ FIX: Reset feedback state!
    s_voice.feedback_state = 0.f;
    
    // Reset filter
    s_voice.filter_z1 = 0.f;
    s_voice.filter_z2 = 0.f;
    
    // Set frequency
    s_voice.base_freq = osc_w0f_for_note(note, 0);
    
    // Activate
    s_voice.active = true;
    s_voice.note_on_time = 0;
}

__unit_callback void unit_note_off(uint8_t note) {
    (void)note;
    
    // ✅ PLAY MODE: Don't stop sequencer! Let it keep playing
    if (s_seq_playing) {
        return;  // Sequencer keeps running
    }
    
    // ✅ OFF/RECORD MODE: Normal note off
    // FM synth uses AD envelopes, just let decay finish
}

__unit_callback void unit_all_note_off() {
    // ✅ PLAY MODE: Don't stop sequencer on all notes off
    if (!s_seq_playing) {
        s_voice.active = false;
        for (int i = 0; i < 4; i++) {
            s_voice.operators[i].active = false;
        }
    }
    // In PLAY mode, sequencer keeps running
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
        case 0: // Algorithm
            s_algorithm = (uint8_t)value;
            if (s_algorithm > 7) s_algorithm = 7;
            break;
            
        case 1: // FM Amount
            // ✅ FIX: Safe FM range (0-1.0 instead of 0-5.0)
            s_fm_amount = valf;
            break;
            
        case 2: // Frequency Ratio
            // ✅ FIX: Reduced range for stability (0.5× to 4×)
            s_freq_ratio = 0.5f + valf * 3.5f;  // 0.5× to 4×
            break;
            
        case 3: // Feedback
            // ✅ FIX: Max 70% for stability
            s_feedback = valf * 0.7f;  // ✅ Max 70%
            break;
            
        case 4: // Attack
            s_attack = valf;
            break;
            
        case 5: // Decay
            s_decay = valf;
            break;
            
        case 6: // Filter
            s_filter_cutoff = valf;
            break;
            
        case 7: // Resonance
            s_filter_resonance = valf;
            break;
            
        case 8: // ✅ PLAY/STOP button
            s_seq_playing = (value != 0);
            
            // ✅ Auto-start when PLAY is turned ON
            if (s_seq_playing) {
                s_seq.current_step = 0;
                s_seq.step_counter = 0;
                s_seq.running = true;
            } else {
                s_seq.running = false;
            }
            break;
            
        case 9: // Step Edit
            s_seq_step_edit = (uint8_t)value;
            if (s_seq_step_edit >= SEQ_STEPS) s_seq_step_edit = 0;
            break;
            
        default:
            break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return s_algorithm;
        case 1: return (int32_t)(s_fm_amount * 1023.f);
        case 2: return (int32_t)(((s_freq_ratio - 0.5f) / 3.5f) * 1023.f);
        case 3: return (int32_t)((s_feedback / 0.7f) * 1023.f);
        case 4: return (int32_t)(s_attack * 1023.f);
        case 5: return (int32_t)(s_decay * 1023.f);
        case 6: return (int32_t)(s_filter_cutoff * 700.f);  // ✅ FIX: Max 700 (was 1023)
        case 7: return (int32_t)(s_filter_resonance * 1023.f);
        case 8: return s_seq_playing ? 1 : 0;
        case 9: return s_seq_step_edit;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 0 && value >= 0 && value < 8) {
        return algo_names[value];
    }
    
    if (id == 8) {
        return (value != 0) ? "ON" : "OFF";
    }
    
    if (id == 9 && value >= 0 && value < 16) {
        // Step number (1-16 instead of 0-15 for display)
        static char step_str[8];
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
    
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    // Extract BPM from fixed point format
    float bpm = (float)(tempo >> 16) + (float)(tempo & 0xFFFF) / 65536.f;
    bpm = clipminmaxf(60.f, bpm, 240.f);
    
    // Calculate samples per step (16th notes)
    // Formula: (60 / BPM) * sample_rate / 4
    s_seq.samples_per_step = (uint32_t)((60.f / bpm) * 48000.f / 4.f);
    s_seq.samples_per_step = clipminmaxu32(3000, s_seq.samples_per_step, 48000);
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    (void)counter;
    
    // Sync sequencer to MIDI clock (4PPQN = 16th notes)
    if (s_seq_playing && s_seq.running) {
        // Reset step counter on clock tick
        s_seq.step_counter = 0;
    }
}

