/*
    ARP KUT V2 - Ultimate Arpeggiator Oscillator
    
    NEW IN V2:
    - Auto-loop (infinite repeat!)
    - Sound characters (House bells, Techno bells, Dance tunes)
    - Fixed is_finite bug
    - Play/pause via note on/off
*/

#include "unit_osc.h"
#include "osc_api.h"
#include "utils/float_math.h"
#include "fx_api.h"

// ========== NaN/Inf CHECK MACRO ==========
#define is_finite(x) ((x) != (x) ? false : ((x) <= 1e10f && (x) >= -1e10f))

// ========== PATTERNS ==========

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

enum WaveShape {
    SHAPE_SAW = 0,
    SHAPE_PULSE,
    SHAPE_TRI,
    SHAPE_SINE
};

const char* shape_names[4] = {"SAW", "PULSE", "TRI", "SINE"};

// ========== NEW: SOUND CHARACTERS ==========

enum SoundCharacter {
    CHAR_STANDARD = 0,      // Classic arp sound
    CHAR_HOUSE_BELLS,       // House bells (FM-like)
    CHAR_TECHNO_BELLS,      // Techno bells (metallic)
    CHAR_DANCE_TUNES,       // Dance tunes (piano-like)
    CHAR_PLUCK,             // Plucked strings
    CHAR_WARM               // Warm analog
};

const char* character_names[6] = {
    "STANDR", "HOUSBL", "TECHNBL", "DANC", "PLUCK", "WARM"
};

// ========== STATE ==========

struct ArpState {
    uint8_t step;
    uint8_t steps_total;
    uint32_t sample_count;
    uint32_t samples_per_step;
    int8_t direction;
    float phase;
    float gate_env;
    bool note_active;
    float base_pitch;
    int8_t drunk_offset;
    bool looping;  // NEW: Loop mode
};

static ArpState s_arp;

// ========== PARAMETERS ==========

static ArpPattern s_pattern = ARP_UP;
static uint8_t s_octaves = 2;
static uint8_t s_steps = 8;
static float s_gate = 0.75f;
static float s_swing = 0.5f;
static float s_accent = 0.3f;
static WaveShape s_shape = SHAPE_SAW;
static float s_detune = 0.2f;
static float s_sub = 0.2f;
static SoundCharacter s_character = CHAR_STANDARD;  // NEW: Replaces filter

static bool s_active = false;

// ========== RANDOM ==========

static uint32_t s_rand = 12345;

inline float randf() {
    s_rand = s_rand * 1103515245 + 12345;
    return (float)(s_rand & 0x7FFF) / 32768.f;
}

// ========== WAVEFORMS ==========

inline float wave_saw(float phase) {
    return 2.f * phase - 1.f;
}

inline float wave_pulse(float phase, float pw) {
    return (phase < pw) ? 1.f : -1.f;
}

inline float wave_tri(float phase) {
    return (phase < 0.5f) ? (4.f * phase - 1.f) : (3.f - 4.f * phase);
}

inline float wave_sine(float phase) {
    return osc_sinf(phase);
}

// ========== NEW: SOUND CHARACTER PROCESSING ==========

inline float apply_character(float phase, float base_output) {
    switch (s_character) {
        case CHAR_HOUSE_BELLS: {
            // FM-style house bells
            float mod = osc_sinf(phase * 3.5f);
            float carrier = osc_sinf(phase + mod * 0.3f);
            float harmonic = osc_sinf(phase * 2.f) * 0.2f;
            return (carrier + harmonic) * 0.7f;
        }
        
        case CHAR_TECHNO_BELLS: {
            // Metallic techno bells
            float bell = osc_sinf(phase);
            float h2 = osc_sinf(phase * 3.2f) * 0.3f;
            float h3 = osc_sinf(phase * 5.7f) * 0.2f;
            return (bell + h2 + h3) * 0.6f;
        }
        
        case CHAR_DANCE_TUNES: {
            // Piano-like dance tunes
            float fund = wave_tri(phase);
            float h2 = osc_sinf(phase * 2.f) * 0.3f;
            float h3 = osc_sinf(phase * 3.f) * 0.15f;
            return (fund + h2 + h3) * 0.6f;
        }
        
        case CHAR_PLUCK: {
            // Plucked string
            float pluck = wave_saw(phase);
            float decay = 1.f - (s_arp.gate_env * 0.3f);
            return pluck * decay;
        }
        
        case CHAR_WARM: {
            // Warm analog
            float saw = wave_saw(phase);
            float sine = osc_sinf(phase);
            return (saw * 0.5f + sine * 0.5f) * 0.8f;
        }
        
        case CHAR_STANDARD:
        default:
            return base_output;
    }
}

// ========== ARP LOGIC ==========

inline int8_t get_arp_note_offset() {
    uint8_t step = s_arp.step % s_steps;
    int8_t offset = 0;
    
    switch (s_pattern) {
        case ARP_UP:
            offset = (step * 12) / s_steps;
            if (s_octaves > 1) {
                offset += (step / s_steps) * 12;
            }
            break;
            
        case ARP_DOWN:
            offset = ((s_steps - step - 1) * 12) / s_steps;
            if (s_octaves > 1) {
                offset += ((s_steps - step - 1) / s_steps) * 12;
            }
            break;
            
        case ARP_UP_DOWN:
            {
                uint8_t half = s_steps / 2;
                if (step < half) {
                    offset = (step * 12) / half;
                } else {
                    offset = ((s_steps - step) * 12) / half;
                }
            }
            break;
            
        case ARP_DOWN_UP:
            {
                uint8_t half = s_steps / 2;
                if (step < half) {
                    offset = ((half - step) * 12) / half;
                } else {
                    offset = ((step - half) * 12) / half;
                }
            }
            break;
            
        case ARP_OCTAVES:
            offset = (step % s_octaves) * 12;
            break;
            
        case ARP_RANDOM:
            {
                uint32_t seed = step * 137 + 157;
                offset = (seed % (s_octaves * 12));
            }
            break;
            
        case ARP_DRUNK:
            {
                if (randf() > 0.5f) {
                    s_arp.drunk_offset++;
                    if (s_arp.drunk_offset > 12) s_arp.drunk_offset = 12;
                } else {
                    s_arp.drunk_offset--;
                    if (s_arp.drunk_offset < -12) s_arp.drunk_offset = -12;
                }
                offset = s_arp.drunk_offset;
            }
            break;
            
        // FIXED: ARP_SPIRAL implementation!
        case ARP_SPIRAL:
            {
                // Expanding spiral pattern: 0, 2, 4, 6, 3, 5, 7, 9, 6, 8, 10, 12...
                uint8_t cycle = step / 4;  // Which cycle (0, 1, 2, ...)
                uint8_t pos = step % 4;    // Position in cycle (0-3)
                offset = (cycle * 3) + (pos * 2);  // Expanding intervals
                // FIXED: Better modulo to prevent issues
                uint8_t max_range = s_octaves * 12;
                if (max_range > 0) {
                    offset = offset % max_range;
                } else {
                    offset = offset % 12;  // Fallback
                }
            }
            break;
            
        case ARP_THIRDS:
            offset = (step % 3) * 4;  // Major third = 4 semitones
            break;
            
        case ARP_FIFTHS:
            offset = (step % 2) * 7;  // Perfect fifth = 7 semitones
            break;
            
        case ARP_BOUNCE:
            {
                uint8_t pos = step % 8;
                if (pos == 0 || pos == 2 || pos == 6) {
                    offset = (step / 8) * 12;
                } else {
                    offset = ((step / 8) * 12) - 3;
                }
            }
            break;
            
        case ARP_STUTTER:
            {
                uint8_t base = step / 2;
                offset = (base * 12) / s_steps;
            }
            break;
            
        case ARP_SKIP:
            {
                if (step % 2 == 0) {
                    offset = ((step / 2) * 12) / s_steps;
                } else {
                    offset = -1;  // Skip this step
                }
            }
            break;
            
        case ARP_DOUBLE:
            {
                uint8_t base = step / 2;
                offset = (base * 12) / s_steps;
            }
            break;
            
        case ARP_EUCLIDEAN:
            {
                uint8_t pulses = 5;
                uint8_t steps = 8;
                bool hit = ((step % steps) * pulses % steps) < pulses;
                if (hit) {
                    offset = ((step / steps) * 12) / s_steps;
                } else {
                    offset = -1;  // Skip
                }
            }
            break;
            
        // FIXED: ARP_BROKEN implementation!
        case ARP_BROKEN:
            {
                // Broken chord arpeggio: root, 3rd, 5th, octave pattern
                const int8_t intervals[4] = {0, 4, 7, 12};  // Root, major 3rd, perfect 5th, octave
                offset = intervals[step % 4];
                // FIXED: Better octave calculation
                if (s_octaves > 0) {
                    offset += ((step / 4) % s_octaves) * 12;
                }
            }
            break;
            
        default:
            offset = (step * 12) / s_steps;
            break;
    }
    
    return clipminmaxi32(-24, offset, 24);
}

// ========== OSCILLATOR ==========

inline float generate_arp_osc() {
    if (!s_active || !s_arp.looping) return 0.f;
    
    // Update arp step
    s_arp.sample_count++;
    
    uint32_t step_length = s_arp.samples_per_step;
    
    // Swing
    if (s_arp.step % 2 == 1) {
        step_length = (uint32_t)((float)step_length * (0.75f + s_swing * 0.5f));
    }
    
    if (s_arp.sample_count >= step_length) {
        s_arp.sample_count = 0;
        s_arp.step++;
        
        // NEW: Loop infinitely!
        if (s_arp.step >= s_steps) {
            s_arp.step = 0;  // Reset to beginning
        }
        
        s_arp.gate_env = 0.f;
    }
    
    // Get note offset
    int8_t note_offset = get_arp_note_offset();
    
    // Skip if offset is -1 (skip pattern)
    if (note_offset == -1) {
        s_arp.gate_env = 0.f;
        return 0.f;
    }
    
    // Gate envelope
    float gate_phase = (float)s_arp.sample_count / (float)step_length;
    float target_gate = (gate_phase < s_gate) ? 1.f : 0.f;
    
    // Accent
    if (s_arp.step % 4 == 0) {
        target_gate *= 1.f + s_accent;
    }
    
    // Smooth envelope
    if (target_gate > s_arp.gate_env) {
        s_arp.gate_env += (target_gate - s_arp.gate_env) * 0.1f;
    } else {
        s_arp.gate_env += (target_gate - s_arp.gate_env) * 0.02f;
    }
    
    // Calculate pitch
    float pitch_ratio = fx_pow2f((float)note_offset / 12.f);
    float freq = s_arp.base_pitch * pitch_ratio;
    float w0 = freq / 48000.f;
    
    // Update phase
    s_arp.phase += w0;
    if (s_arp.phase >= 1.f) s_arp.phase -= 1.f;
    
    // Generate base wave
    float output = 0.f;
    
    switch (s_shape) {
        case SHAPE_SAW:
            output = wave_saw(s_arp.phase);
            break;
        case SHAPE_PULSE:
            output = wave_pulse(s_arp.phase, 0.5f);
            break;
        case SHAPE_TRI:
            output = wave_tri(s_arp.phase);
            break;
        case SHAPE_SINE:
            output = wave_sine(s_arp.phase);
            break;
    }
    
    // Apply sound character
    output = apply_character(s_arp.phase, output);
    
    // Detune layer
    if (s_detune > 0.01f) {
        float detune_phase = s_arp.phase + s_detune * 0.01f;
        if (detune_phase >= 1.f) detune_phase -= 1.f;
        output += wave_saw(detune_phase) * s_detune * 0.3f;
    }
    
    // Sub
    if (s_sub > 0.01f) {
        float sub_phase = s_arp.phase * 0.5f;
        while (sub_phase >= 1.f) sub_phase -= 1.f;
        while (sub_phase < 0.f) sub_phase += 1.f;
        output += wave_sine(sub_phase) * s_sub;
    }
    
    // Apply gate envelope
    output *= s_arp.gate_env;
    
    // Validate (NaN/Inf check)
    if (!is_finite(output)) output = 0.f;
    
    return clipminmaxf(-1.f, output, 1.f);
}

// ========== CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    
    s_arp.step = 0;
    s_arp.sample_count = 0;
    s_arp.samples_per_step = 6000;
    s_arp.phase = 0.f;
    s_arp.gate_env = 0.f;
    s_arp.direction = 1;
    s_arp.drunk_offset = 0;
    s_arp.looping = false;  // NEW: Loop mode
    
    s_pattern = ARP_UP;
    s_octaves = 2;
    s_steps = 8;
    s_gate = 0.75f;
    s_swing = 0.5f;
    s_accent = 0.3f;
    s_shape = SHAPE_SAW;
    s_detune = 0.2f;
    s_sub = 0.2f;
    s_character = CHAR_STANDARD;  // NEW: Character instead of filter
    
    s_active = false;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}
__unit_callback void unit_reset() {
    s_arp.step = 0;
    s_arp.sample_count = 0;
    s_arp.phase = 0.f;
    s_arp.gate_env = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    (void)in;
    
    for (uint32_t f = 0; f < frames; f++) {
        out[f] = generate_arp_osc();
    }
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
    s_arp.base_pitch = osc_notehzf(note);
    
    // NEW: Toggle play/pause
    if (s_active && s_arp.looping) {
        // If already playing, pause
        s_arp.looping = false;
    } else {
        // Start playing
        s_active = true;
        s_arp.looping = true;
        s_arp.step = 0;
        s_arp.sample_count = 0;
        s_arp.gate_env = 0.f;
        s_arp.drunk_offset = 0;
    }
    
    (void)velocity;
}

__unit_callback void unit_note_off(uint8_t note) {
    (void)note;
    // Don't stop on note off - let it loop!
    // User can stop by pressing note again (toggle) or all notes off
}

__unit_callback void unit_all_note_off() {
    s_active = false;
    s_arp.looping = false;  // NEW: Stop looping
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

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_pattern = (ArpPattern)value; break;
        case 1: s_octaves = (uint8_t)value; break;
        case 2: s_steps = (uint8_t)value; break;
        case 3: s_gate = valf; break;
        case 4: s_swing = valf; break;
        case 5: s_accent = valf; break;
        case 6: s_shape = (WaveShape)value; break;
        case 7: s_detune = valf; break;
        case 8: s_sub = valf; break;
        case 9: s_character = (SoundCharacter)value; break;  // NEW: Character
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)s_pattern;
        case 1: return (int32_t)s_octaves;
        case 2: return (int32_t)s_steps;
        case 3: return (int32_t)(s_gate * 1023.f);
        case 4: return (int32_t)(s_swing * 1023.f);
        case 5: return (int32_t)(s_accent * 1023.f);
        case 6: return (int32_t)s_shape;
        case 7: return (int32_t)(s_detune * 1023.f);
        case 8: return (int32_t)(s_sub * 1023.f);
        case 9: return (int32_t)s_character;  // NEW: Character
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 0 && value >= 0 && value < 16) return pattern_names[value];
    if (id == 6 && value >= 0 && value < 4) return shape_names[value];
    if (id == 9 && value >= 0 && value < 6) return character_names[value];  // NEW: Character
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    float bpm = (float)(tempo >> 16) + (float)(tempo & 0xFFFF) / 65536.f;
    s_arp.samples_per_step = (uint32_t)((60.f / bpm) * 48000.f / 4.f);
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    (void)counter;
}

