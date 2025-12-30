/*
    ARP BEAST - Ultimate Arpeggiator Controller
    
    Transform the NTS-1 mkII arpeggiator into a monster!
*/

#include "unit_modfx.h"
#include "fx_api.h"
#include "utils/float_math.h"
#include "utils/int_math.h"

// ========== ARP PATTERNS ==========

enum ArpPattern {
    ARP_UP = 0,
    ARP_DOWN,
    ARP_UP_DOWN,
    ARP_DOWN_UP,
    ARP_RANDOM,
    ARP_DRUNK,
    ARP_OCTAVES,
    ARP_SPIRAL,
    ARP_BOUNCE,
    ARP_STUTTER,
    ARP_SKIP,
    ARP_DOUBLE,
    ARP_THIRDS,
    ARP_FIFTHS,
    ARP_BROKEN,
    ARP_EUCLIDEAN
};

const char* pattern_names[16] = {
    "UP", "DOWN", "UPDOWN", "DOWNUP",
    "RANDOM", "DRUNK", "OCTAVE", "SPIRAL",
    "BOUNCE", "STUTTER", "SKIP", "DOUBLE",
    "THIRDS", "FIFTHS", "BROKEN", "EUCLID"
};

// ========== HARMONY MODES ==========

enum HarmonyMode {
    HARM_NONE = 0,
    HARM_THIRD,
    HARM_FIFTH,
    HARM_OCTAVE,
    HARM_TRIAD,
    HARM_SEVENTH,
    HARM_POWER,
    HARM_CLUSTER
};

const char* harmony_names[8] = {
    "NONE", "3RD", "5TH", "OCT",
    "TRIAD", "7TH", "POWER", "CLUST"
};

// ========== TEMPO MULTIPLIERS ==========

const float tempo_multipliers[8] = {
    0.25f,  // 1/4×
    0.5f,   // 1/2×
    0.75f,  // 3/4×
    1.0f,   // 1×
    1.5f,   // 1.5×
    2.0f,   // 2×
    3.0f,   // 3×
    4.0f    // 4×
};

const char* tempo_names[8] = {
    "1/4X", "1/2X", "3/4X", "1X",
    "1.5X", "2X", "3X", "4X"
};

// ========== ARP STATE ==========

struct ArpNote {
    float velocity;    // Note velocity (0-1)
    float gate;        // Gate length (0-1)
    bool accent;       // Accent flag
    bool active;       // Note active
};

#define MAX_ARP_STEPS 64  // Max pattern length

struct ArpState {
    ArpNote pattern[MAX_ARP_STEPS];
    uint8_t pattern_length;
    uint8_t current_step;
    uint32_t step_counter;
    uint32_t samples_per_step;
    int8_t direction;      // 1 = forward, -1 = reverse
    int8_t drunk_offset;   // For drunk walk
    float phase;           // For modulation
    float envelope;        // ✅ FIX: Smooth envelope state
};

static ArpState s_arp;

// ========== PARAMETERS ==========

static uint8_t s_pattern = ARP_UP;
static uint8_t s_octave_range = 2;
static float s_swing = 0.5f;          // 50% = no swing
static float s_gate_length = 0.75f;   // 75%
static float s_accent_amount = 0.3f;  // 30%
static float s_probability = 1.0f;    // 100%
static uint8_t s_harmony_mode = HARM_NONE;
static uint8_t s_tempo_mult = 3;     // 1×
static float s_randomize = 0.0f;      // 0%
static float s_mix = 1.0f;            // 100%

// Random seed
static uint32_t s_random_seed = 12345;

// ========== RANDOM GENERATOR ==========

inline float random_float() {
    s_random_seed ^= s_random_seed << 13;
    s_random_seed ^= s_random_seed >> 17;
    s_random_seed ^= s_random_seed << 5;
    return (float)(s_random_seed % 10000) / 10000.f;
}

// ========== PATTERN GENERATORS ==========

void generate_pattern_up() {
    s_arp.pattern_length = s_octave_range * 12;
    for (uint8_t i = 0; i < s_arp.pattern_length; i++) {
        s_arp.pattern[i].velocity = 0.8f;
        s_arp.pattern[i].gate = s_gate_length;
        s_arp.pattern[i].accent = (i % 4 == 0);  // Accent every 4th
        s_arp.pattern[i].active = true;
    }
}

void generate_pattern_down() {
    s_arp.pattern_length = s_octave_range * 12;
    for (uint8_t i = 0; i < s_arp.pattern_length; i++) {
        s_arp.pattern[i].velocity = 0.8f;
        s_arp.pattern[i].gate = s_gate_length;
        s_arp.pattern[i].accent = (i % 4 == 0);
        s_arp.pattern[i].active = true;
    }
}

void generate_pattern_up_down() {
    s_arp.pattern_length = s_octave_range * 12 * 2 - 2;
    for (uint8_t i = 0; i < s_arp.pattern_length; i++) {
        s_arp.pattern[i].velocity = 0.8f;
        s_arp.pattern[i].gate = s_gate_length;
        s_arp.pattern[i].accent = (i == 0 || i == s_arp.pattern_length - 1);
        s_arp.pattern[i].active = true;
    }
}

void generate_pattern_random() {
    s_arp.pattern_length = 16;
    for (uint8_t i = 0; i < s_arp.pattern_length; i++) {
        s_arp.pattern[i].velocity = 0.7f + random_float() * 0.3f;
        s_arp.pattern[i].gate = s_gate_length * (0.5f + random_float() * 0.5f);
        s_arp.pattern[i].accent = (random_float() > 0.7f);
        s_arp.pattern[i].active = (random_float() > (1.0f - s_probability));
    }
}

void generate_pattern_octaves() {
    s_arp.pattern_length = s_octave_range;
    for (uint8_t i = 0; i < s_arp.pattern_length; i++) {
        s_arp.pattern[i].velocity = 0.8f;
        s_arp.pattern[i].gate = s_gate_length;
        s_arp.pattern[i].accent = true;
        s_arp.pattern[i].active = true;
    }
}

void generate_pattern_stutter() {
    s_arp.pattern_length = 16;
    uint8_t idx = 0;
    for (uint8_t i = 0; i < 8 && idx < 16; i++) {
        uint8_t repeats = 1 + (uint8_t)(random_float() * 3.f);  // 1-4 repeats
        for (uint8_t r = 0; r < repeats && idx < 16; r++) {
            s_arp.pattern[idx].velocity = 0.8f - r * 0.1f;
            s_arp.pattern[idx].gate = s_gate_length * 0.5f;
            s_arp.pattern[idx].accent = (r == 0);
            s_arp.pattern[idx].active = true;
            idx++;
        }
    }
    s_arp.pattern_length = idx;
}

void generate_pattern_euclidean() {
    uint8_t steps = 16;
    uint8_t pulses = (uint8_t)(s_probability * 12.f);  // 0-12 pulses
    if (pulses == 0) pulses = 1;
    
    s_arp.pattern_length = steps;
    
    // Euclidean rhythm algorithm
    for (uint8_t i = 0; i < steps; i++) {
        bool hit = ((i * pulses) % steps) < pulses;
        s_arp.pattern[i].velocity = hit ? 0.9f : 0.5f;
        s_arp.pattern[i].gate = hit ? s_gate_length : s_gate_length * 0.5f;
        s_arp.pattern[i].accent = hit && (i % 4 == 0);
        s_arp.pattern[i].active = hit;
    }
}

// ========== PATTERN GENERATOR DISPATCHER ==========

void generate_pattern() {
    switch (s_pattern) {
        case ARP_UP:
            generate_pattern_up();
            break;
            
        case ARP_DOWN:
            generate_pattern_down();
            break;
            
        case ARP_UP_DOWN:
        case ARP_DOWN_UP:
            generate_pattern_up_down();
            break;
            
        case ARP_RANDOM:
            generate_pattern_random();
            break;
            
        case ARP_OCTAVES:
            generate_pattern_octaves();
            break;
            
        case ARP_STUTTER:
            generate_pattern_stutter();
            break;
            
        case ARP_EUCLIDEAN:
            generate_pattern_euclidean();
            break;
            
        default:
            // Use UP as fallback
            generate_pattern_up();
            break;
    }
    
    // Apply randomization if enabled
    if (s_randomize > 0.01f) {
        for (uint8_t i = 0; i < s_arp.pattern_length; i++) {
            if (random_float() < s_randomize) {
                s_arp.pattern[i].velocity *= 0.7f + random_float() * 0.6f;
                s_arp.pattern[i].gate *= 0.7f + random_float() * 0.6f;
                s_arp.pattern[i].accent = (random_float() > 0.5f);
            }
        }
    }
}

// ========== ARP PROCESSOR ==========

inline float process_arp_modulation(float input) {
    // Get current step
    ArpNote* step = &s_arp.pattern[s_arp.current_step];
    
    // Calculate target envelope value
    float target_env = 0.f;
    
    if (step->active) {
        // Calculate velocity modulation
        float velocity = step->velocity;
        if (step->accent) {
            velocity += s_accent_amount;
            velocity = clipminmaxf(0.f, velocity, 1.f);
        }
        
        // Calculate gate phase
        float gate_phase = (float)s_arp.step_counter / (float)s_arp.samples_per_step;
        
        // Target envelope: gate * velocity
        if (gate_phase < step->gate) {
            target_env = velocity;
        } else {
            // ✅ FIX: Smooth fade out instead of instant cut
            float fade_phase = (gate_phase - step->gate) / (1.f - step->gate);
            target_env = velocity * (1.f - fade_phase * 0.5f);  // Fade to 50% instead of 0
        }
    } else {
        // ✅ FIX: Inactive steps fade to 10% instead of instant cut
        target_env = 0.1f;
    }
    
    // ✅ FIX: Smooth envelope transitions (prevents clicks!)
    const float attack_rate = 0.05f;   // Fast attack
    const float release_rate = 0.02f;  // Slower release
    
    if (target_env > s_arp.envelope) {
        // Attack: fast rise
        s_arp.envelope += (target_env - s_arp.envelope) * attack_rate;
    } else {
        // Release: smooth fall
        s_arp.envelope += (target_env - s_arp.envelope) * release_rate;
    }
    
    // Ensure envelope is finite
    if (!si_isfinite(s_arp.envelope)) {
        s_arp.envelope = 0.f;
    }
    
    // Apply envelope to input
    return input * s_arp.envelope;
}

// ========== MAIN PROCESSING ==========

inline void advance_arp_step() {
    // Advance step counter
    s_arp.step_counter++;
    
    // Apply swing on odd steps
    uint32_t step_length = s_arp.samples_per_step;
    if (s_arp.current_step % 2 == 1) {
        float swing_offset = (s_swing - 0.5f) * 0.5f;  // ±25%
        step_length = (uint32_t)((float)step_length * (1.f + swing_offset));
    }
    
    // Check if we need to advance
    if (s_arp.step_counter >= step_length) {
        s_arp.step_counter = 0;
        
        // Advance pattern
        switch (s_pattern) {
            case ARP_UP:
            case ARP_RANDOM:
            case ARP_STUTTER:
            case ARP_EUCLIDEAN:
            case ARP_OCTAVES:
                s_arp.current_step++;
                if (s_arp.current_step >= s_arp.pattern_length) {
                    s_arp.current_step = 0;
                }
                break;
                
            case ARP_DOWN:
                if (s_arp.current_step == 0) {
                    s_arp.current_step = s_arp.pattern_length - 1;
                } else {
                    s_arp.current_step--;
                }
                break;
                
            case ARP_UP_DOWN:
                s_arp.current_step += s_arp.direction;
                if (s_arp.current_step >= s_arp.pattern_length) {
                    s_arp.current_step = s_arp.pattern_length - 2;
                    s_arp.direction = -1;
                } else if ((int8_t)s_arp.current_step < 0) {
                    s_arp.current_step = 1;
                    s_arp.direction = 1;
                }
                break;
                
            case ARP_DOWN_UP:
                s_arp.current_step -= s_arp.direction;
                if ((int8_t)s_arp.current_step < 0) {
                    s_arp.current_step = 1;
                    s_arp.direction = -1;
                } else if (s_arp.current_step >= s_arp.pattern_length) {
                    s_arp.current_step = s_arp.pattern_length - 2;
                    s_arp.direction = 1;
                }
                break;
                
            default:
                s_arp.current_step++;
                if (s_arp.current_step >= s_arp.pattern_length) {
                    s_arp.current_step = 0;
                }
                break;
        }
    }
}

// ========== UNIT CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;
    
    // Init arp state
    s_arp.current_step = 0;
    s_arp.step_counter = 0;
    s_arp.samples_per_step = 6000;  // 120 BPM, 16th notes
    s_arp.direction = 1;
    s_arp.drunk_offset = 0;
    s_arp.phase = 0.f;
    s_arp.pattern_length = 0;
    s_arp.envelope = 1.f;  // ✅ FIX: Initialize envelope
    
    // Init parameters
    s_pattern = ARP_UP;
    s_octave_range = 2;
    s_swing = 0.5f;
    s_gate_length = 0.75f;
    s_accent_amount = 0.3f;
    s_probability = 1.0f;
    s_harmony_mode = HARM_NONE;
    s_tempo_mult = 3;  // 1×
    s_randomize = 0.0f;
    s_mix = 1.0f;
    
    // Generate initial pattern
    generate_pattern();
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    s_arp.current_step = 0;
    s_arp.step_counter = 0;
    s_arp.envelope = 1.f;  // ✅ FIX: Reset envelope
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    for (uint32_t f = 0; f < frames; f++) {
        float in_l = in[f * 2];
        float in_r = in[f * 2 + 1];
        
        // Advance arp
        advance_arp_step();
        
        // Process arp modulation
        float arp_l = process_arp_modulation(in_l);
        float arp_r = process_arp_modulation(in_r);
        
        // Mix dry/wet
        float out_l = in_l * (1.f - s_mix) + arp_l * s_mix;
        float out_r = in_r * (1.f - s_mix) + arp_r * s_mix;
        
        // Output
        out[f * 2] = clipminmaxf(-1.f, out_l, 1.f);
        out[f * 2 + 1] = clipminmaxf(-1.f, out_r, 1.f);
    }
}

// ========== PARAMETER HANDLING ==========

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: // Pattern
            s_pattern = (uint8_t)value;
            generate_pattern();
            s_arp.current_step = 0;
            break;
            
        case 1: // Octaves
            s_octave_range = (uint8_t)value;
            generate_pattern();
            break;
            
        case 2: // Swing
            s_swing = valf;
            break;
            
        case 3: // Gate
            s_gate_length = valf;
            generate_pattern();
            break;
            
        case 4: // Accent
            s_accent_amount = valf;
            break;
            
        case 5: // Probability
            s_probability = valf;
            generate_pattern();
            break;
            
        case 6: // Harmony
            s_harmony_mode = (uint8_t)value;
            break;
            
        case 7: // Tempo
            s_tempo_mult = (uint8_t)value;
            break;
            
        case 8: // Randomize
            s_randomize = valf;
            generate_pattern();
            break;
            
        case 9: // Mix
            s_mix = valf;
            break;
            
        default:
            break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return s_pattern;
        case 1: return s_octave_range;
        case 2: return (int32_t)(s_swing * 1023.f);
        case 3: return (int32_t)(s_gate_length * 1023.f);
        case 4: return (int32_t)(s_accent_amount * 1023.f);
        case 5: return (int32_t)(s_probability * 1023.f);
        case 6: return s_harmony_mode;
        case 7: return s_tempo_mult;
        case 8: return (int32_t)(s_randomize * 1023.f);
        case 9: return (int32_t)(s_mix * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 0 && value >= 0 && value < 16) {
        return pattern_names[value];
    }
    if (id == 6 && value >= 0 && value < 8) {
        return harmony_names[value];
    }
    if (id == 7 && value >= 0 && value < 8) {
        return tempo_names[value];
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    float bpm = (float)(tempo >> 16) + (float)(tempo & 0xFFFF) / 65536.f;
    bpm = clipminmaxf(60.f, bpm, 240.f);
    
    // Calculate samples per step with tempo multiplier
    float multiplier = tempo_multipliers[s_tempo_mult];
    s_arp.samples_per_step = (uint32_t)((60.f / bpm) * 48000.f / 4.f / multiplier);
    s_arp.samples_per_step = clipminmaxu32(1000, s_arp.samples_per_step, 48000);
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    (void)counter;
    // Sync to MIDI clock
    s_arp.step_counter = 0;
}

