/*
    HYPERPOLY ULTIMATE - Maximum Edition
    10 parameters + 16-step sequencer!
*/

#include "unit_osc.h"
#include "osc_api.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "fx_api.h"  // For fastertanh2f

// ========== CHORD LIBRARY ==========

const float chord_ratios[12][4] = {
    {1.00f, 1.00f, 1.00f, 0.50f},  // 0: MONO
    {1.00f, 2.00f, 4.00f, 0.50f},  // 1: OCTAVES
    {1.00f, 1.50f, 2.00f, 0.50f},  // 2: FIFTH
    {1.00f, 1.26f, 1.50f, 0.50f},  // 3: MAJOR
    {1.00f, 1.19f, 1.50f, 0.50f},  // 4: MINOR
    {1.00f, 1.12f, 1.50f, 0.50f},  // 5: DIM
    {1.00f, 1.26f, 1.68f, 0.50f},  // 6: AUG
    {1.00f, 1.33f, 1.50f, 0.50f},  // 7: SUS4
    {1.00f, 1.12f, 1.50f, 0.50f},  // 8: SUS2
    {1.00f, 1.26f, 1.50f, 1.78f},  // 9: MAJ7
    {1.00f, 1.19f, 1.50f, 1.68f},  // 10: MIN7
    {1.00f, 1.26f, 1.50f, 1.68f},  // 11: DOM7
};

const char* chord_names[12] = {
    "MONO", "OCT", "5TH", "MAJ", "MIN", "DIM",
    "AUG", "SUS4", "SUS2", "MAJ7", "MIN7", "DOM7"
};

// ========== SEQUENCER ==========

#define SEQ_STEPS 16

enum SequencerMode {
    SEQ_OFF = 0,
    SEQ_PLAY = 1,
    SEQ_RECORD = 2
};

struct SequencerStep {
    uint8_t note;
    uint8_t velocity;
    bool active;
};

struct Sequencer {
    SequencerStep steps[SEQ_STEPS];
    uint8_t current_step;
    uint8_t length;
    uint32_t step_counter;
    uint32_t samples_per_step;
    bool running;
    uint8_t last_played_note;
};

static Sequencer s_seq;

// ========== VOICE STATE ==========

struct Voice {
    float phase[4];
    float w0;
    float filter_z1;  // ✅ Filter state
    float pwm_phase;  // ✅ PWM LFO
    bool active;
};

static Voice s_voice;

// ========== PARAMETERS ==========

static uint8_t s_chord_type = 3;
static float s_detune = 0.5f;
static float s_sub_mix = 0.5f;
static float s_brightness = 0.75f;
static uint8_t s_voice_count = 4;     // ✅ NEW
static float s_phase_offset = 0.0f;   // ✅ NEW
static float s_pwm_depth = 0.2f;      // ✅ NEW
static float s_filter_cutoff = 1.0f;  // ✅ NEW
static bool s_seq_playing = false;    // ✅ PLAY/STOP button
static bool s_seq_recording = false;  // ✅ REC mode (via note_on in REC)
static uint8_t s_seq_step_edit = 0;

// ========== POLY BLEP ==========

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

inline float get_pwm_width() {
    if (s_pwm_depth < 0.01f) return 0.5f;
    
    // LFO at ~3 Hz
    s_voice.pwm_phase += 3.f / 48000.f;
    if (s_voice.pwm_phase >= 1.f) s_voice.pwm_phase -= 1.f;
    
    float lfo = osc_sinf(s_voice.pwm_phase);
    float width = 0.5f + lfo * s_pwm_depth * 0.4f;  // 0.1 to 0.9
    return clipminmaxf(0.1f, width, 0.9f);
}

// ========== LOW-PASS FILTER ==========

inline float process_filter(float input) {
    if (s_filter_cutoff > 0.99f) return input;  // Bypass
    
    // One-pole LP filter
    float cutoff = s_filter_cutoff * s_filter_cutoff;  // Exponential curve
    cutoff = clipminmaxf(0.01f, cutoff, 0.99f);
    
    s_voice.filter_z1 += cutoff * (input - s_voice.filter_z1);
    
    // Denormal kill
    if (si_fabsf(s_voice.filter_z1) < 1e-15f) s_voice.filter_z1 = 0.f;
    
    return s_voice.filter_z1;
}

// ========== OSCILLATOR ==========

inline float generate_oscillator() {
    if (!s_voice.active) return 0.f;
    
    const float *ratios = chord_ratios[s_chord_type];
    float sum = 0.f;
    
    // ✅ Voice count control
    int active_voices = (int)s_voice_count;
    
    for (int v = 0; v < active_voices; v++) {
        float ratio = ratios[v];
        
        // Detune
        if (v > 0) {
            float detune_cents = (v - 1.5f) * s_detune * 20.f;
            ratio *= fastpow2f(detune_cents / 1200.f);
        }
        
        float w = s_voice.w0 * ratio;
        w = clipminmaxf(0.0001f, w, 0.45f);
        
        // ✅ Phase offset
        float p = s_voice.phase[v] + (float)v * s_phase_offset * 0.25f;
        while (p >= 1.f) p -= 1.f;
        while (p < 0.f) p += 1.f;
        
        // Sawtooth
        float saw = (2.f * p - 1.f);
        saw -= poly_blep(p, w);
        
        // ✅ Pulse with PWM
        float pulse_width = get_pwm_width();
        float pulse = (p < pulse_width) ? 1.f : -1.f;
        pulse += poly_blep(p, w);
        
        float p_shifted = p + (1.f - pulse_width);
        if (p_shifted >= 1.f) p_shifted -= 1.f;
        pulse -= poly_blep(p_shifted, w);
        
        // Mix
        float osc = pulse * (1.f - s_brightness) + saw * s_brightness;
        
        // Sub mix
        if (v == 3) osc *= s_sub_mix;
        
        sum += osc;
        
        // Advance phase
        s_voice.phase[v] += w;
        if (s_voice.phase[v] >= 1.f) s_voice.phase[v] -= 1.f;
    }
    
    // Normalize by active voices
    sum /= (float)active_voices;
    
    // ✅ Apply filter
    sum = process_filter(sum);
    
    return sum;
}

// ========== SEQUENCER PROCESSOR ==========

inline void process_sequencer() {
    if (!s_seq_playing || !s_seq.running) return;
    
    s_seq.step_counter++;
    
    if (s_seq.step_counter >= s_seq.samples_per_step) {
        s_seq.step_counter = 0;
        
        // Stop previous note
        if (s_seq.last_played_note > 0) {
            s_voice.active = false;
        }
        
        // Get current step
        SequencerStep* step = &s_seq.steps[s_seq.current_step];
        
        // Trigger note
        if (step->active && step->note > 0) {
            for (int i = 0; i < 4; i++) {
                s_voice.phase[i] = 0.f;
            }
            
            s_voice.w0 = osc_w0f_for_note(step->note, 0);
            s_voice.active = true;
            s_seq.last_played_note = step->note;
        }
        
        // Advance step
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
    if (desc->input_channels != 2 || desc->output_channels != 1) return k_unit_err_geometry;
    
    // Init voice
    s_voice.active = false;
    s_voice.w0 = 0.f;
    s_voice.filter_z1 = 0.f;
    s_voice.pwm_phase = 0.f;
    for (int i = 0; i < 4; i++) {
        s_voice.phase[i] = 0.f;
    }
    
    // Init parameters
    s_chord_type = 3;
    s_detune = 0.5f;
    s_sub_mix = 0.5f;
    s_brightness = 0.75f;
    s_voice_count = 4;
    s_phase_offset = 0.0f;
    s_pwm_depth = 0.2f;
    s_filter_cutoff = 1.0f;
    
    // Init sequencer
    s_seq.current_step = 0;
    s_seq.length = 16;
    s_seq.step_counter = 0;
    s_seq.samples_per_step = 12000;  // 120 BPM
    s_seq.running = false;
    s_seq.last_played_note = 0;
    
    for (int i = 0; i < SEQ_STEPS; i++) {
        s_seq.steps[i].note = 0;
        s_seq.steps[i].velocity = 100;
        s_seq.steps[i].active = false;
    }
    
    // Default pattern: C major scale
    s_seq.steps[0].note = 60;  s_seq.steps[0].active = true;
    s_seq.steps[1].note = 62;  s_seq.steps[1].active = true;
    s_seq.steps[2].note = 64;  s_seq.steps[2].active = true;
    s_seq.steps[3].note = 65;  s_seq.steps[3].active = true;
    s_seq.steps[4].note = 67;  s_seq.steps[4].active = true;
    s_seq.steps[5].note = 69;  s_seq.steps[5].active = true;
    s_seq.steps[6].note = 71;  s_seq.steps[6].active = true;
    s_seq.steps[7].note = 72;  s_seq.steps[7].active = true;
    
    s_seq_playing = false;
    s_seq_recording = false;
    s_seq_step_edit = 0;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    s_voice.active = false;
    s_voice.filter_z1 = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    (void)in;
    
    for (uint32_t f = 0; f < frames; f++) {
        // ✅ Process sequencer
        process_sequencer();
        
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
    
    // ✅ OFF MODE: Normal operation (also triggers voice)
    (void)velocity;
    
    for (int i = 0; i < 4; i++) {
        s_voice.phase[i] = 0.f;
    }
    
    s_voice.w0 = osc_w0f_for_note(note, 0);
    s_voice.active = true;
}

__unit_callback void unit_note_off(uint8_t note) {
    (void)note;
    
    // ✅ Keep sequencer running in PLAY mode
    if (s_seq_playing) {
        return;  // Don't stop!
    }
    
    s_voice.active = false;
}

__unit_callback void unit_all_note_off() {
    // ✅ Keep sequencer running in PLAY mode
    if (s_seq_playing) {
        return;  // Don't stop!
    }
    
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
        case 0: // Chord Type
            s_chord_type = (uint8_t)value;
            break;
            
        case 1: // Detune
            s_detune = valf;
            break;
            
        case 2: // Sub Mix
            s_sub_mix = valf;
            break;
            
        case 3: // Brightness
            s_brightness = valf;
            break;
            
        case 4: // ✅ Voice Count
            s_voice_count = (uint8_t)value;
            if (s_voice_count < 1) s_voice_count = 1;
            if (s_voice_count > 4) s_voice_count = 4;
            break;
            
        case 5: // ✅ Phase Offset
            s_phase_offset = valf;
            break;
            
        case 6: // ✅ PWM Depth
            s_pwm_depth = valf;
            break;
            
        case 7: // ✅ Filter Cutoff
            s_filter_cutoff = valf;
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
            
        case 9: // ✅ Step Edit
            s_seq_step_edit = (uint8_t)value;
            if (s_seq_step_edit >= SEQ_STEPS) s_seq_step_edit = 0;
            break;
            
        default:
            break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return s_chord_type;
        case 1: return (int32_t)(s_detune * 1023.f);
        case 2: return (int32_t)(s_sub_mix * 1023.f);
        case 3: return (int32_t)(s_brightness * 1023.f);
        case 4: return s_voice_count;
        case 5: return (int32_t)(s_phase_offset * 1023.f);
        case 6: return (int32_t)(s_pwm_depth * 1023.f);
        case 7: return (int32_t)(s_filter_cutoff * 1023.f);
        case 8: return s_seq_playing ? 1 : 0;
        case 9: return s_seq_step_edit;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 0 && value >= 0 && value < 12) {
        return chord_names[value];
    }
    
    if (id == 4 && value >= 1 && value <= 4) {
        // Manual string conversion for voice count
        static char voice_str[4];
        int voice_num = value;
        voice_str[0] = '0' + voice_num;
        voice_str[1] = '\0';
        return voice_str;
    }
    
    if (id == 8) {
        return (value != 0) ? "ON" : "OFF";
    }
    
    if (id == 9 && value >= 0 && value < 16) {
        // Manual string conversion for step number
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
    
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    float bpm = (float)(tempo >> 16) + (float)(tempo & 0xFFFF) / 65536.f;
    bpm = clipminmaxf(60.f, bpm, 240.f);
    
    s_seq.samples_per_step = (uint32_t)((60.f / bpm) * 48000.f / 4.f);
    s_seq.samples_per_step = clipminmaxu32(3000, s_seq.samples_per_step, 48000);
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    (void)counter;
    
    if (s_seq_playing && s_seq.running) {
        s_seq.step_counter = 0;
    }
}
