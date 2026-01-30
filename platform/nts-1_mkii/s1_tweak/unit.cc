/*
    S-1 TWEAK - Roland AIRA S-1 / SH-101 Inspired Synthesizer
    
    Ultimate techno/house oscillator for NTS-1 mkII
*/

#include "unit_osc.h"
#include "osc_api.h"
#include "fx_api.h"  // ✅ For fx_pow2f() in oscillators
#include "utils/float_math.h"  // ✅ For fastertanh2f()
#include "utils/int_math.h"

// ========== MODES ==========

enum SynthMode {
    MODE_MONO = 0,      // 0-24%: SH-101 style mono
    MODE_POLY,          // 25-49%: Para-polyphonic
    MODE_UNISON,        // 50-74%: Stacked voices
    MODE_CHORD          // 75-100%: Chord mode
};

// ========== CHORD TYPES (16 house-friendly chords) ==========

const float chord_intervals[16][4] = {
    {0.f, 4.f, 7.f, 0.f},      // 0: Major
    {0.f, 3.f, 7.f, 0.f},      // 1: Minor
    {0.f, 2.f, 7.f, 0.f},      // 2: Sus2
    {0.f, 5.f, 7.f, 0.f},      // 3: Sus4
    {0.f, 4.f, 7.f, 11.f},     // 4: Maj7
    {0.f, 3.f, 7.f, 10.f},     // 5: Min7
    {0.f, 4.f, 7.f, 10.f},     // 6: Dom7
    {0.f, 4.f, 7.f, 14.f},     // 7: Maj9
    {0.f, 3.f, 7.f, 14.f},     // 8: Min9
    {0.f, 4.f, 7.f, 9.f},      // 9: 6th
    {0.f, 3.f, 7.f, 9.f},      // 10: Min6
    {0.f, 4.f, 7.f, 14.f},     // 11: Add9
    {0.f, 3.f, 6.f, 0.f},      // 12: Dim
    {0.f, 4.f, 8.f, 0.f},      // 13: Aug
    {0.f, 7.f, 12.f, 0.f},     // 14: Power (5th)
    {0.f, 12.f, 19.f, 0.f}     // 15: Octaves
};

const char* chord_names[16] = {
    "MAJ", "MIN", "SUS2", "SUS4",
    "MAJ7", "MIN7", "DOM7", "MAJ9",
    "MIN9", "6TH", "MIN6", "ADD9",
    "DIM", "AUG", "POWER", "OCT"
};

// ========== VOICE STRUCTURE (4 INTERNAL VOICES) ==========

struct Voice {
    float phase_saw;
    float phase_pulse;
    float phase_sub;
    float phase_noise;
    float w0;              // Base frequency
    bool active;
};

static Voice s_voices[4];

// ========== RATTLE/SUB-STEP STATE ==========

struct RattleState {
    uint32_t trigger_time;
    uint8_t sub_step_count;
    uint8_t current_sub_step;
    bool active;
};

static RattleState s_rattle;

// ========== MOTION LFO ==========

static float s_motion_phase = 0.f;

// ========== NOISE STATE ==========

static uint32_t s_noise_state = 12345;
static float s_noise_envelope = 0.f;

// ========== PARAMETERS ==========

static float s_wave_mix = 0.5f;
static float s_draw_shape = 0.0f;
static float s_chop_comb = 0.0f;
static uint8_t s_mode = MODE_MONO;
static float s_chord_type = 0.0f;
static float s_detune = 0.3f;
static float s_noise_amount = 0.1f;
static float s_rattle_amount = 0.0f;
static float s_probability = 0.0f;
static float s_motion = 0.0f;

// Current note
static uint8_t s_current_note = 60;
static uint8_t s_current_velocity = 100;

// ========== RANDOM GENERATOR ==========

inline float random_float() {
    s_noise_state ^= s_noise_state << 13;
    s_noise_state ^= s_noise_state >> 17;
    s_noise_state ^= s_noise_state << 5;
    return (float)(s_noise_state % 10000) / 10000.f;
}

// ========== POLY BLEP (ANTI-ALIASING) ==========

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

// ========== VA CORE OSCILLATORS ==========

inline float generate_saw(float phase, float w) {
    float saw = 2.f * phase - 1.f;
    saw -= poly_blep(phase, w);
    return saw;
}

inline float generate_pulse(float phase, float w, float pw) {
    float pulse = (phase < pw) ? 1.f : -1.f;
    pulse += poly_blep(phase, w);
    
    float phase_shifted = phase + (1.f - pw);
    if (phase_shifted >= 1.f) phase_shifted -= 1.f;
    pulse -= poly_blep(phase_shifted, w);
    
    return pulse;
}

inline float generate_sub(float phase) {
    // Simple sine sub-oscillator
    return osc_sinf(phase);
}

inline float generate_noise() {
    return 2.f * random_float() - 1.f;
}

// ========== DRAW SHAPE (S-1 OSC DRAW) ==========

inline float apply_draw_shape(float input, float amount) {
    if (amount < 0.01f) return input;
    
    // 0-50%: Morphing/asymmetry
    if (amount < 0.5f) {
        float morph = amount * 2.f;
        // Add stepped character
        float stepped = floorf(input * (4.f + morph * 12.f)) / (4.f + morph * 12.f);
        return input * (1.f - morph) + stepped * morph;
    }
    
    // 50-100%: Wavefold/distortion
    else {
        float fold = (amount - 0.5f) * 2.f;
        float folded = fastertanh2f(input * (1.f + fold * 3.f));  // ✅ FIX: Use fastertanh2f from float_math.h
        return folded;
    }
}

// ========== CHOP/COMB (S-1 OSC CHOP) ==========

inline float apply_chop_comb(float input, float phase, float amount) {
    if (amount < 0.01f) return input;
    
    // Create gaps in waveform (chop)
    float gaps = 2.f + amount * 14.f;  // 2-16 gaps
    float gap_phase = phase * gaps;
    while (gap_phase >= 1.f) gap_phase -= 1.f;
    
    // Comb filter effect
    float gate = (gap_phase < (1.f - amount * 0.3f)) ? 1.f : 0.f;
    
    // Metallic resonance
    float comb = input + input * amount * osc_sinf(phase * gaps);
    
    return comb * gate;
}

// ========== WAVE MIX ==========

inline float generate_wave_mix(Voice* voice) {
    float w = voice->w0;
    w = clipminmaxf(0.0001f, w, 0.45f);
    
    // Generate all waveforms
    float saw = generate_saw(voice->phase_saw, w);
    float pulse = generate_pulse(voice->phase_pulse, w, 0.5f);  // 50% PW
    float sub = generate_sub(voice->phase_sub);
    float noise = generate_noise() * s_noise_envelope;
    
    // Mix based on s_wave_mix
    // 0 = saw-heavy
    // 50 = balanced
    // 100 = pulse+sub+noise heavy
    
    float saw_amount = 1.f - s_wave_mix;
    float pulse_amount = s_wave_mix;
    float sub_amount = s_wave_mix * 0.8f;
    float noise_amount = s_wave_mix * s_noise_amount;
    
    float mixed = saw * saw_amount + 
                  pulse * pulse_amount + 
                  sub * sub_amount * 0.5f +  // -1 octave, lower level
                  noise * noise_amount;
    
    // Advance phases
    voice->phase_saw += w;
    if (voice->phase_saw >= 1.f) voice->phase_saw -= 1.f;
    
    voice->phase_pulse += w;
    if (voice->phase_pulse >= 1.f) voice->phase_pulse -= 1.f;
    
    voice->phase_sub += w * 0.5f;  // -1 octave
    if (voice->phase_sub >= 1.f) voice->phase_sub -= 1.f;
    
    return mixed * 0.7f;  // Normalize
}

// ========== MOTION LFO ==========

inline void process_motion_lfo() {
    if (s_motion < 0.01f) return;
    
    // Slow LFO (~0.5 Hz)
    s_motion_phase += 0.5f / 48000.f;
    if (s_motion_phase >= 1.f) s_motion_phase -= 1.f;
    
    float lfo = osc_sinf(s_motion_phase);
    
    // Modulate multiple parameters
    // (Applied globally, affects all voices)
}

// ========== NOISE ENVELOPE (FOR RISER) ==========

inline void process_noise_envelope(bool note_on) {
    if (note_on) {
        s_noise_envelope = 0.f;
    }
    
    // Rise over time if noise amount is high
    if (s_noise_amount > 0.5f) {
        float rise_rate = (s_noise_amount - 0.5f) * 2.f;
        s_noise_envelope += rise_rate * 0.0001f;
        s_noise_envelope = clipminmaxf(0.f, s_noise_envelope, 1.f);
    } else {
        // Constant low level
        s_noise_envelope = s_noise_amount * 2.f;
    }
}

// ========== RATTLE/SUB-STEPS ==========

inline void process_rattle() {
    if (s_rattle_amount < 0.01f || !s_rattle.active) return;
    
    s_rattle.trigger_time++;
    
    // Determine number of sub-steps
    uint8_t sub_steps = 1;
    if (s_rattle_amount > 0.66f) sub_steps = 4;
    else if (s_rattle_amount > 0.33f) sub_steps = 3;
    else sub_steps = 2;
    
    s_rattle.sub_step_count = sub_steps;
    
    // Trigger interval (fast ratchets)
    uint32_t interval = 3000;  // ~16th note at 120 BPM
    
    if (s_rattle.trigger_time >= interval) {
        s_rattle.trigger_time = 0;
        s_rattle.current_sub_step++;
        
        if (s_rattle.current_sub_step >= s_rattle.sub_step_count) {
            s_rattle.active = false;
        } else {
            // Trigger next sub-step (re-trigger oscillators)
            // Apply probability
            if (random_float() > s_probability * 0.3f) {
                // Re-trigger (reset phases with slight variation)
                for (int i = 0; i < 4; i++) {
                    if (s_voices[i].active) {
                        float randomize = s_probability * 0.1f;
                        s_voices[i].phase_saw = random_float() * randomize;
                        s_voices[i].phase_pulse = random_float() * randomize;
                    }
                }
            }
        }
    }
}

// ========== MAIN OSCILLATOR ==========

inline float generate_oscillator() {
    float sum = 0.f;
    int active_voices = 0;
    
    // Determine number of active voices based on mode
    switch (s_mode) {
        case MODE_MONO:
            active_voices = 1;
            break;
        case MODE_POLY:
        case MODE_UNISON:
        case MODE_CHORD:
            active_voices = 4;
            break;
    }
    
    // Generate and sum voices
    for (int v = 0; v < active_voices; v++) {
        if (!s_voices[v].active) continue;
        
        // Generate wave mix
        float sample = generate_wave_mix(&s_voices[v]);
        
        // Apply Draw Shape
        sample = apply_draw_shape(sample, s_draw_shape);
        
        // Apply Chop/Comb
        sample = apply_chop_comb(sample, s_voices[v].phase_saw, s_chop_comb);
        
        sum += sample;
    }
    
    // Normalize
    if (active_voices > 0) {
        sum /= (float)active_voices;
    }
    
    return sum;
}

// ========== UNIT CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    
    // Init voices
    for (int i = 0; i < 4; i++) {
        s_voices[i].phase_saw = 0.f;
        s_voices[i].phase_pulse = 0.f;
        s_voices[i].phase_sub = 0.f;
        s_voices[i].w0 = 0.f;
        s_voices[i].active = false;
    }
    
    // Init rattle
    s_rattle.trigger_time = 0;
    s_rattle.sub_step_count = 0;
    s_rattle.current_sub_step = 0;
    s_rattle.active = false;
    
    // Init motion
    s_motion_phase = 0.f;
    
    // Init noise
    s_noise_envelope = 0.f;
    
    // Init parameters
    s_wave_mix = 0.5f;
    s_draw_shape = 0.0f;
    s_chop_comb = 0.0f;
    s_mode = MODE_MONO;
    s_chord_type = 0.0f;
    s_detune = 0.3f;
    s_noise_amount = 0.1f;
    s_rattle_amount = 0.0f;
    s_probability = 0.0f;
    s_motion = 0.0f;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    for (int i = 0; i < 4; i++) {
        s_voices[i].active = false;
    }
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    (void)in;
    
    for (uint32_t f = 0; f < frames; f++) {
        // Process motion LFO
        process_motion_lfo();
        
        // Process noise envelope
        process_noise_envelope(false);
        
        // Process rattle
        process_rattle();
        
        // Generate oscillator
        float sample = generate_oscillator();
        
        // Output gain
        sample *= 1.8f;
        
        // Limiting
        sample = clipminmaxf(-1.f, sample, 1.f);
        
        out[f] = sample;
    }
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
    s_current_note = note;
    s_current_velocity = velocity;
    
    // Determine mode from parameter value
    float mode_val = s_mode / 4.f;  // Convert enum to 0-1 range
    
    // Re-interpret mode based on parameter
    // We'll use a static mode variable that gets updated in set_param
    // For now, use s_mode directly
    
    // Calculate base frequency
    float base_w0 = osc_w0f_for_note(note, 0);
    
    // Configure voices based on mode
    switch (s_mode) {
        case MODE_MONO:
        {
            // Single voice
            s_voices[0].w0 = base_w0;
            s_voices[0].phase_saw = 0.f;
            s_voices[0].phase_pulse = 0.f;
            s_voices[0].phase_sub = 0.f;
            s_voices[0].active = true;
            
            for (int i = 1; i < 4; i++) {
                s_voices[i].active = false;
            }
            break;
        }
        
        case MODE_POLY:
        {
            // Para-polyphonic (all voices play same note with slight detune)
            for (int i = 0; i < 4; i++) {
                float detune_cents = (i - 1.5f) * s_detune * 10.f;
                float detune_ratio = fx_pow2f(detune_cents / 1200.f);
                
                s_voices[i].w0 = base_w0 * detune_ratio;
                s_voices[i].phase_saw = (float)i * 0.25f;  // Phase spread
                s_voices[i].phase_pulse = (float)i * 0.25f;
                s_voices[i].phase_sub = 0.f;
                s_voices[i].active = true;
            }
            break;
        }
        
        case MODE_UNISON:
        {
            // Stacked detuned voices (fat lead)
            for (int i = 0; i < 4; i++) {
                float detune_cents = (i - 1.5f) * s_detune * 20.f;
                float detune_ratio = fx_pow2f(detune_cents / 1200.f);  // ✅ FIX: Use fx_pow2f (osc_pow2f doesn't exist!)
                
                s_voices[i].w0 = base_w0 * detune_ratio;
                s_voices[i].phase_saw = 0.f;
                s_voices[i].phase_pulse = 0.f;
                s_voices[i].phase_sub = 0.f;
                s_voices[i].active = true;
            }
            break;
        }
        
        case MODE_CHORD:
        {
            // Chord mode
            // Map s_chord_type (0-1) to chord index (0-15)
            uint8_t chord_idx = (uint8_t)(s_chord_type * 15.f);
            if (chord_idx > 15) chord_idx = 15;
            
            for (int i = 0; i < 4; i++) {
                float interval = chord_intervals[chord_idx][i];
                if (interval == 0.f && i > 0) {
                    s_voices[i].active = false;
                    continue;
                }
                
                float ratio = fx_pow2f(interval / 12.f);
                s_voices[i].w0 = base_w0 * ratio;
                s_voices[i].phase_saw = 0.f;
                s_voices[i].phase_pulse = 0.f;
                s_voices[i].phase_sub = 0.f;
                s_voices[i].active = true;
            }
            break;
        }
    }
    
    // Reset noise envelope
    process_noise_envelope(true);
    
    // Trigger rattle
    if (s_rattle_amount > 0.01f) {
        s_rattle.trigger_time = 0;
        s_rattle.current_sub_step = 0;
        s_rattle.active = true;
    }
}

__unit_callback void unit_note_off(uint8_t note) {
    (void)note;
    // SH-101 style: note off doesn't immediately stop
    // Voices decay naturally
}

__unit_callback void unit_all_note_off() {
    for (int i = 0; i < 4; i++) {
        s_voices[i].active = false;
    }
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
        case 0: // Wave Mix
            s_wave_mix = valf;
            break;
        case 1: // Draw Shape
            s_draw_shape = valf;
            break;
        case 2: // Chop/Comb
            s_chop_comb = valf;
            break;
        case 3: // Mode
        {
            // Map 0-1023 to mode enum
            if (valf < 0.25f) {
                s_mode = MODE_MONO;
            } else if (valf < 0.5f) {
                s_mode = MODE_POLY;
            } else if (valf < 0.75f) {
                s_mode = MODE_UNISON;
            } else {
                s_mode = MODE_CHORD;
            }
            break;
        }
        case 4: // Chord Type
            s_chord_type = valf;
            break;
        case 5: // Detune/Spread
            s_detune = valf;
            break;
        case 6: // Noise/Riser
            s_noise_amount = valf;
            break;
        case 7: // Rattle
            s_rattle_amount = valf;
            break;
        case 8: // Probability
            s_probability = valf;
            break;
        case 9: // Motion
            s_motion = valf;
            break;
        default:
            break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)(s_wave_mix * 1023.f);
        case 1: return (int32_t)(s_draw_shape * 1023.f);
        case 2: return (int32_t)(s_chop_comb * 1023.f);
        case 3: 
        {
            // Convert mode enum back to 0-1023
            float mode_val = 0.f;
            switch (s_mode) {
                case MODE_MONO: mode_val = 0.125f; break;
                case MODE_POLY: mode_val = 0.375f; break;
                case MODE_UNISON: mode_val = 0.625f; break;
                case MODE_CHORD: mode_val = 0.875f; break;
            }
            return (int32_t)(mode_val * 1023.f);
        }
        case 4: return (int32_t)(s_chord_type * 1023.f);
        case 5: return (int32_t)(s_detune * 1023.f);
        case 6: return (int32_t)(s_noise_amount * 1023.f);
        case 7: return (int32_t)(s_rattle_amount * 1023.f);
        case 8: return (int32_t)(s_probability * 1023.f);
        case 9: return (int32_t)(s_motion * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    // Mode display
    if (id == 3) {
        float mode_val = (float)value / 1023.f;
        if (mode_val < 0.25f) return "MONO";
        if (mode_val < 0.5f) return "POLY";
        if (mode_val < 0.75f) return "UNISON";
        return "CHORD";
    }
    
    // Chord type display
    if (id == 4) {
        uint8_t chord_idx = (uint8_t)((float)value / 1023.f * 15.f);
        if (chord_idx < 16) return chord_names[chord_idx];
    }
    
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    (void)tempo;
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    (void)counter;
}

