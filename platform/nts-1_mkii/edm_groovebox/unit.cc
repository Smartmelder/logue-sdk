/*
EDM GROOVEBOX - HYBRID SEQUENCER + OSCILLATOR

ARCHITECTURE:

1. KICK DRUM SEQUENCER (always active, defines tempo)
   - 16-step pattern
   - 4 patterns: 1-3, 1-2-3-4, 1-3 + offbeat, four-on-floor
   - Analog-style kick synthesis (sine + click)

2. CHORD PROGRESSION ENGINE
   - 8 progressions: Minor cycles, III-vi-ii-V, EDM I-V-vi-IV, etc.
   - Auto-transpose to played note
   - 3-voice polyphonic chords

3. PERCUSSION LAYER
   - Claps (2-4 beat patterns)
   - Hats (8th/16th notes)
   - Density control

4. SEQUENCER
   - 16 steps
   - 1-4 bar loops
   - BPM control (80-160)
   - Humanize (timing variation)

PARAMETERS:
0. Kick Pattern (0-3)
1. Chord Progression (0-7)
2. Clap Density (0-100%)
3. Hat Density (0-100%)
4. Kick Volume (0-100%)
5. Chord Volume (0-100%)
6. Percussion Volume (0-100%)
7. BPM (80-160)
8. Loop Bars (1-4)
9. Humanize (0-100%)
*/

#include "unit_osc.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "osc_api.h"

// SDK compatibility - PI is already defined in CMSIS arm_math.h
#ifndef PI
#define PI 3.14159265359f
#endif

#define MAX_VOICES 3
#define SEQUENCER_STEPS 16
#define SINE_TABLE_SIZE 256

static const unit_runtime_osc_context_t *s_context;

// Sine table
static float s_sine_table[SINE_TABLE_SIZE];

// ========== KICK DRUM SYNTHESIS ==========
struct KickDrum {
    float phase;
    float pitch_env;
    float amp_env;
    uint32_t counter;
    bool active;
};

static KickDrum s_kick;

// ========== CHORD VOICE ==========
struct ChordVoice {
    float phase;
    float amp;
    uint8_t note;
    bool active;
};

// ========== PERCUSSION ==========
struct Percussion {
    // Clap
    float clap_phase;
    float clap_env;
    uint32_t clap_counter;
    bool clap_active;
    
    // Hat
    float hat_phase;
    float hat_env;
    uint32_t hat_counter;
    bool hat_active;
};

static Percussion s_perc;

// ========== SEQUENCER STATE ==========
struct Sequencer {
    uint32_t step;              // Current step (0-15)
    uint32_t sample_counter;    // Sample counter for timing
    uint32_t samples_per_step;  // Samples per 16th note
    float humanize_offset[SEQUENCER_STEPS]; // Timing variations
};

static Sequencer s_seq;

// ========== CHORD PROGRESSIONS ==========
// Stored as semitone offsets from root
struct ChordProgression {
    int8_t chords[4][3];  // 4 chords, 3 notes each
    const char* name;
};

static const ChordProgression s_chord_progs[8] = {
    {{{0, 3, 7}, {0, 5, 9}, {0, 7, 10}, {0, 3, 7}}, "i-iv-v"},      // Minor cycle
    {{{0, 4, 7}, {0, 9, 16}, {0, 2, 9}, {0, 7, 11}}, "III-vi-ii-V"}, // Jazz
    {{{0, 4, 7}, {0, 7, 11}, {0, 9, 16}, {0, 5, 9}}, "I-V-vi-IV"},   // Pop/EDM
    {{{0, 3, 7}, {0, 7, 10}, {0, 5, 9}, {0, 3, 7}}, "i-v-iv"},       // Dark
    {{{0, 4, 7}, {0, 5, 9}, {0, 7, 11}, {0, 4, 7}}, "I-IV-V"},       // Classic
    {{{0, 3, 7}, {0, 10, 15}, {0, 5, 9}, {0, 7, 11}}, "i-VI-iv-V"},  // Epic
    {{{0, 4, 7}, {0, 3, 7}, {0, 5, 9}, {0, 7, 11}}, "I-i-IV-V"},     // Major/Minor
    {{{0, 7, 12}, {0, 5, 12}, {0, 7, 14}, {0, 5, 12}}, "5th Power"}  // Power chords
};

// ========== KICK PATTERNS ==========
// 16-step patterns (1 = kick, 0 = no kick)
static const uint8_t s_kick_patterns[4][SEQUENCER_STEPS] = {
    {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0},  // 1-3 (classic house)
    {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0},  // 1-2-3-4
    {1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0},  // 1-3 + offbeat
    {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}   // Four-on-floor
};

// Clap pattern (hits on 2 and 4)
static const uint8_t s_clap_pattern[SEQUENCER_STEPS] = {
    0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0
};

// ========== PARAMETERS ==========
static uint8_t s_kick_pattern = 0;
static uint8_t s_chord_prog = 0;
static float s_clap_density = 0.5f;
static float s_hat_density = 0.5f;
static float s_kick_volume = 0.75f;
static float s_chord_volume = 0.5f;
static float s_perc_volume = 0.4f;
static uint8_t s_bpm = 120;
static uint8_t s_loop_bars = 1;  // 0=1bar, 1=2bars, 2=3bars, 3=4bars
static float s_humanize = 0.25f;

static ChordVoice s_chord_voices[MAX_VOICES];
static uint8_t s_root_note = 60;  // C4 default

// ✅ NEW: Latch mode state
static bool s_sequencer_running = false;  // Is sequencer active?
static bool s_latch_mode = true;          // Latch mode enabled (keeps running)

// ========== HELPER FUNCTIONS ==========

void init_sine_table() {
    for (int i = 0; i < SINE_TABLE_SIZE; i++) {
        float phase = (float)i / (float)SINE_TABLE_SIZE;
        s_sine_table[i] = osc_sinf(phase);
    }
}

inline float sine_lookup(float phase) {
    phase -= (int32_t)phase;
    if (phase < 0.f) phase += 1.f;
    float idx_f = phase * (float)(SINE_TABLE_SIZE - 1);
    uint32_t idx0 = (uint32_t)idx_f;
    uint32_t idx1 = (idx0 + 1) % SINE_TABLE_SIZE;
    float frac = idx_f - (float)idx0;
    return s_sine_table[idx0] * (1.f - frac) + s_sine_table[idx1] * frac;
}

// FIXED: Use fastpow2f (correct SDK function from utils/float_math.h)
inline float fast_exp(float x) {
    // Fast exponential approximation
    if (x < -10.f) return 0.f;
    return fastpow2f(x * 1.44269504f);  // 1/ln(2) - fastpow2f from float_math.h
}

// FIXED: Use osc_white() from SDK or local implementation
static uint32_t s_noise_seed = 123456789;

inline float white_noise() {
    // Use SDK function if available, otherwise local implementation
    s_noise_seed = s_noise_seed * 1103515245u + 12345u;
    return ((float)(s_noise_seed >> 16) / 32768.f) - 1.f;
}

// FIXED: Add fast_tanh function (like in tr909)
inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// ========== KICK DRUM SYNTHESIS ==========
inline void trigger_kick() {
    s_kick.phase = 0.f;
    s_kick.pitch_env = 1.f;
    s_kick.amp_env = 1.f;
    s_kick.counter = 0;
    s_kick.active = true;
}

inline float process_kick() {
    if (!s_kick.active) return 0.f;
    
    float t_sec = (float)s_kick.counter / 48000.f;
    
    // Pitch envelope (starts at 200Hz, drops to 50Hz)
    float pitch_start = 200.f;
    float pitch_end = 50.f;
    float pitch_decay = 0.05f;  // 50ms
    s_kick.pitch_env = fast_exp(-t_sec / pitch_decay);
    float pitch = pitch_end + (pitch_start - pitch_end) * s_kick.pitch_env;
    
    // Amplitude envelope
    float amp_decay = 0.4f;  // 400ms
    s_kick.amp_env = fast_exp(-t_sec / amp_decay);
    
    // Sine wave kick body
    // FIXED: Use PI instead of M_PI
    float w0 = 2.f * PI * pitch / 48000.f;
    s_kick.phase += w0;
    if (s_kick.phase >= 2.f * PI) s_kick.phase -= 2.f * PI;
    float phase_norm = s_kick.phase / (2.f * PI);
    if (phase_norm >= 1.f) phase_norm -= 1.f;
    if (phase_norm < 0.f) phase_norm += 1.f;
    float sine = osc_sinf(phase_norm);
    
    // Click envelope (very short)
    float click_env = (t_sec < 0.002f) ? (1.f - t_sec / 0.002f) : 0.f;
    float click = white_noise() * click_env * 0.3f;
    
    float output = (sine * s_kick.amp_env + click) * s_kick_volume;
    
    s_kick.counter++;
    if (s_kick.amp_env < 0.001f) s_kick.active = false;
    
    return output;
}

// ========== CLAP SYNTHESIS ==========
inline void trigger_clap() {
    s_perc.clap_phase = 0.f;
    s_perc.clap_env = 1.f;
    s_perc.clap_counter = 0;
    s_perc.clap_active = true;
}

inline float process_clap() {
    if (!s_perc.clap_active) return 0.f;
    
    float t_sec = (float)s_perc.clap_counter / 48000.f;
    
    // Multi-hit envelope (3 hits)
    float env = 0.f;
    float hit_times[3] = {0.f, 0.01f, 0.02f};
    for (int i = 0; i < 3; i++) {
        float t_hit = t_sec - hit_times[i];
        if (t_hit > 0.f && t_hit < 0.05f) {
            env += fast_exp(-t_hit / 0.02f);
        }
    }
    
    // Bandpassed noise (clap sound)
    float noise = white_noise();
    // Simple HPF
    static float clap_hpf_z = 0.f;
    float hp_cutoff = 800.f / 48000.f;
    clap_hpf_z = clap_hpf_z + hp_cutoff * (noise - clap_hpf_z);
    float clap_sig = noise - clap_hpf_z;
    
    float output = clap_sig * env * s_perc_volume * 0.6f;
    
    s_perc.clap_counter++;
    if (t_sec > 0.1f) s_perc.clap_active = false;
    
    return output;
}

// ========== HAT SYNTHESIS ==========
inline void trigger_hat() {
    s_perc.hat_phase = 0.f;
    s_perc.hat_env = 1.f;
    s_perc.hat_counter = 0;
    s_perc.hat_active = true;
}

inline float process_hat() {
    if (!s_perc.hat_active) return 0.f;
    
    float t_sec = (float)s_perc.hat_counter / 48000.f;
    
    // Short envelope
    float decay = 0.05f;  // 50ms
    s_perc.hat_env = fast_exp(-t_sec / decay);
    
    // Filtered noise (hi-hat sound)
    float noise = white_noise();
    // Simple HPF
    static float hat_hpf_z = 0.f;
    float hp_cutoff = 5000.f / 48000.f;
    hat_hpf_z = hat_hpf_z + hp_cutoff * (noise - hat_hpf_z);
    float hat_sig = noise - hat_hpf_z;
    
    float output = hat_sig * s_perc.hat_env * s_perc_volume * 0.4f;
    
    s_perc.hat_counter++;
    if (s_perc.hat_env < 0.001f) s_perc.hat_active = false;
    
    return output;
}

// ========== CHORD SYNTHESIS ==========
inline void trigger_chord(uint8_t chord_index) {
    const ChordProgression *prog = &s_chord_progs[s_chord_prog];
    
    for (int v = 0; v < MAX_VOICES; v++) {
        ChordVoice *voice = &s_chord_voices[v];
        voice->note = s_root_note + prog->chords[chord_index][v];
        voice->phase = 0.f;
        voice->amp = 1.f;
        voice->active = true;
    }
}

inline float process_chords() {
    float sig = 0.f;
    
    for (int v = 0; v < MAX_VOICES; v++) {
        ChordVoice *voice = &s_chord_voices[v];
        if (!voice->active) continue;
        
        float w0 = osc_w0f_for_note(voice->note, 0);
        voice->phase += w0;
        if (voice->phase >= 1.f) voice->phase -= 1.f;
        
        // Saw wave
        float saw = (voice->phase * 2.f - 1.f);
        
        // Envelope (sustain for 1 bar)
        voice->amp *= 0.9998f;  // Slow decay
        
        sig += saw * voice->amp;
    }
    
    return sig * s_chord_volume * 0.33f;  // Normalize for 3 voices
}

// ========== SEQUENCER ==========
inline void update_sequencer_timing() {
    // Calculate samples per 16th note
    float seconds_per_beat = 60.f / (float)s_bpm;
    float seconds_per_16th = seconds_per_beat / 4.f;
    s_seq.samples_per_step = (uint32_t)(seconds_per_16th * 48000.f);
    
    // Generate humanize offsets
    for (int i = 0; i < SEQUENCER_STEPS; i++) {
        float rand = ((float)(i * 997 % 1000) / 1000.f - 0.5f);  // Pseudo-random
        s_seq.humanize_offset[i] = rand * s_humanize * 0.05f * (float)s_seq.samples_per_step;
    }
}

inline void sequencer_step() {
    uint8_t pattern_idx = s_kick_pattern;
    
    // Trigger kick
    if (s_kick_patterns[pattern_idx][s_seq.step]) {
        trigger_kick();
    }
    
    // Trigger clap (density controlled)
    if (s_clap_pattern[s_seq.step]) {
        float rand = (float)((s_seq.step * 123) % 100) / 100.f;
        if (rand < s_clap_density) {
            trigger_clap();
        }
    }
    
    // Trigger hat (every 8th or 16th note based on density)
    if (s_seq.step % 2 == 0) {  // 8th notes
        float rand = (float)((s_seq.step * 456) % 100) / 100.f;
        if (rand < s_hat_density) {
            trigger_hat();
        }
    }
    
    // Trigger chord (every 4 steps = quarter note)
    if (s_seq.step % 4 == 0) {
        uint8_t chord_index = (s_seq.step / 4) % 4;
        trigger_chord(chord_index);
    }
    
    // Advance step
    s_seq.step = (s_seq.step + 1) % SEQUENCER_STEPS;
}

// ========== SDK CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 1) return k_unit_err_geometry;
    
    s_context = static_cast<const unit_runtime_osc_context_t*>(desc->hooks.runtime_context);
    
    init_sine_table();
    
    // Init kick
    s_kick.active = false;
    
    // Init percussion
    s_perc.clap_active = false;
    s_perc.hat_active = false;
    
    // Init chords
    for (int v = 0; v < MAX_VOICES; v++) {
        s_chord_voices[v].active = false;
    }
    
    // Init sequencer
    s_seq.step = 0;
    s_seq.sample_counter = 0;
    update_sequencer_timing();
    
    // ✅ NEW: Init latch state
    s_sequencer_running = false;  // Starts stopped
    s_latch_mode = true;           // Latch ON by default
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    s_seq.step = 0;
    s_seq.sample_counter = 0;
}

__unit_callback void unit_resume() {}

__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    // Get root note from NTS-1
    uint8_t base_note = (s_context->pitch >> 8) & 0xFF;
    
    // ✅ Update root note only if playing (latch mode)
    if (s_sequencer_running) {
        s_root_note = base_note;
    }
    
    for (uint32_t f = 0; f < frames; f++) {
        // ✅ Only run sequencer if active (latch mode)
        if (s_sequencer_running) {
            // Sequencer timing
            uint32_t step_length = s_seq.samples_per_step + (int32_t)s_seq.humanize_offset[s_seq.step];
            if (s_seq.sample_counter >= step_length) {
                s_seq.sample_counter = 0;
                sequencer_step();
            }
            s_seq.sample_counter++;
        }
        
        // Process all elements
        float kick_sig = process_kick();
        float clap_sig = process_clap();
        float hat_sig = process_hat();
        
        // ✅ Generate chords (ONLY if sequencer running!)
        float chord_sig = 0.f;
        if (s_sequencer_running) {
            chord_sig = process_chords();
        }
        
        // Mix
        float sig = kick_sig + clap_sig + hat_sig + chord_sig;
        
        // Soft clip
        sig = fast_tanh(sig * 1.5f);
        
        // FIXED: Increased output volume from 2.5f to 16.0f (like tr909)
        out[f] = clipminmaxf(-1.f, sig * 16.0f, 1.f);
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_kick_pattern = value; break;
        case 1: s_chord_prog = value; break;
        case 2: s_clap_density = valf; break;
        case 3: s_hat_density = valf; break;
        case 4: s_kick_volume = valf; break;
        case 5: s_chord_volume = valf; break;
        case 6: s_perc_volume = valf; break;
        case 7: 
            // Convert 0-1023 (0-100%) to 80-160 BPM
            s_bpm = 80 + (uint8_t)(valf * 80.f);
            if (s_bpm < 80) s_bpm = 80;
            if (s_bpm > 160) s_bpm = 160;
            update_sequencer_timing();
            break;
        case 8: s_loop_bars = value; break;
        case 9: 
            s_humanize = valf;
            update_sequencer_timing();
            break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return s_kick_pattern;
        case 1: return s_chord_prog;
        case 2: return (int32_t)(s_clap_density * 1023.f);
        case 3: return (int32_t)(s_hat_density * 1023.f);
        case 4: return (int32_t)(s_kick_volume * 1023.f);
        case 5: return (int32_t)(s_chord_volume * 1023.f);
        case 6: return (int32_t)(s_perc_volume * 1023.f);
        case 7: return (int32_t)((s_bpm - 80) * 1023.f / 80.f);  // Convert BPM back to 0-1023
        case 8: return s_loop_bars;
        case 9: return (int32_t)(s_humanize * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 0) {
        static const char *kick_names[] = {"1-3", "1234", "1-3+", "4FLR"};
        if (value >= 0 && value < 4) return kick_names[value];
    }
    if (id == 1) {
        if (value >= 0 && value < 8) return s_chord_progs[value].name;
    }
    if (id == 8) {
        static const char *loop_names[] = {"1BAR", "2BAR", "3BAR", "4BAR"};
        if (value >= 0 && value < 4) return loop_names[value];
    }
    return "";
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velo) {
    s_root_note = note;
    
    // ✅ Start sequencer if not running (latch mode)
    if (!s_sequencer_running) {
        s_sequencer_running = true;
        s_seq.step = 0;
        s_seq.sample_counter = 0;
    } else {
        // ✅ If already running, just transpose and reset
        s_seq.step = 0;
        s_seq.sample_counter = 0;
    }
}

__unit_callback void unit_note_off(uint8_t note) {
    // ✅ In latch mode: sequencer keeps running!
    // ✅ In non-latch mode: stop sequencer
    if (!s_latch_mode) {
        s_sequencer_running = false;
        
        // Stop all chord voices
        for (int v = 0; v < MAX_VOICES; v++) {
            s_chord_voices[v].active = false;
        }
    }
    // If latch_mode = true: DO NOTHING, sequencer keeps running!
}

__unit_callback void unit_all_note_off() {
    // ✅ ALWAYS stop sequencer on all_note_off (panic button)
    s_sequencer_running = false;
    
    // Stop all chord voices
    for (int v = 0; v < MAX_VOICES; v++) {
        s_chord_voices[v].active = false;
    }
    
    // Stop kick
    s_kick.active = false;
    
    // Stop percussion
    s_perc.clap_active = false;
    s_perc.hat_active = false;
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    // tempo in BPM × 10 (e.g., 1200 = 120.0 BPM)
    float bpm = (float)tempo / 10.f;
    if (bpm < 60.f) bpm = 120.f;  // Default
    s_bpm = (uint8_t)bpm;
    if (s_bpm < 80) s_bpm = 80;
    if (s_bpm > 160) s_bpm = 160;
    update_sequencer_timing();
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    // ✅ Sync sequencer to MIDI clock (4PPQN = 16th notes)
    // This fixes ARP delay bug by syncing sequencer to external MIDI clock
    if (s_sequencer_running) {
        sequencer_step();
        s_seq.sample_counter = 0;
    }
}

