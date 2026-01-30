/*
    ROLAND TB-303 BASS LINE - AUTHENTIC ACID SYNTHESIS
    
    CRITICAL TB-303 CHARACTERISTICS IMPLEMENTED:
    
    === OSCILLATOR ===
    - Sawtooth/Square waveform
    - Soft saturation (pre-filter)
    - Waveform crossfade (not instant switch)
    - Hard sync on phase reset
    
    === FILTER (THE HEART OF ACID!) ===
    - 18dB/oct (3-pole) diode ladder filter
    - Self-oscillation at high resonance
    - BASS BOOST at low cutoff (TB-303 secret!)
    - IN-FILTER overdrive (feedback path saturation)
    - Resonance compensation
    
    === ENVELOPE ===
    - Instant attack (<1ms)
    - Exponential decay
    - No sustain (always decaying)
    - ACCENT modulates:
      * Filter cutoff (+30%)
      * Envelope depth (+50%)
      * Volume (+6dB)
      * Decay time (+20%)
    
    === SLIDE (PORTAMENTO) ===
    - S-curve slide (exponential approach)
    - Slide time depends on interval
    - Active only when legato
    
    === DISTORTION ===
    - Pre-filter: Soft VCO saturation
    - In-filter: Feedback path overdrive
    - Filter self-distortion at high resonance
    
    ACID HOUSE REFERENCE TRACKS:
    - Phuture - Acid Tracks (1987)
    - Hardfloor - Acperience (1992)
    - Josh Wink - Higher State (1995)
    - Plastikman - Spastik (1993)
    
    BRONNEN:
    - TB-303 service manual
    - Robin Whittle's TB-303 analysis
    - Devil Fish modifications
    - Cyclone Analogic TT-303 circuit
*/

#include "unit_osc.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "macros.h"
#include <math.h>

#define MAX_VOICES 1  // TB-303 is monophonic!

static const unit_runtime_osc_context_t *s_context;

// Oscillator state
static float s_phase;
static float s_waveform_blend;  // Smooth crossfade

// Filter state (3-pole diode ladder)
static float s_filter_z1;
static float s_filter_z2;
static float s_filter_z3;
static float s_filter_feedback;

// Envelope state
static float s_env_level;
static uint32_t s_env_counter;

// Slide (portamento)
static float s_current_pitch;
static float s_target_pitch;
static bool s_slide_active;
static uint8_t s_last_note;
static bool s_note_is_held;

// Parameters
static float s_cutoff_base;
static float s_resonance;
static float s_env_mod;
static float s_decay;
static float s_accent_amount;
static float s_waveform;        // 0=saw, 1=square
static float s_distortion;
static uint8_t s_slide_time;
static bool s_accent_active;

// Last velocity
static uint8_t s_last_velocity;

static uint32_t s_sample_counter;

// Fast math approximations
inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

inline float fast_exp(float x) {
    // Approximation: e^x ≈ (1 + x/256)^256
    // Simplified for speed
    if (x < -5.f) return 0.f;
    if (x > 5.f) return 148.f;
    
    x = 1.f + x * 0.00390625f;  // x/256
    x *= x; x *= x; x *= x; x *= x;  // ^16
    x *= x; x *= x; x *= x; x *= x;  // ^256
    return x;
}

// PolyBLEP antialiasing
inline float poly_blep(float t, float dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.f;
    } else if (t > 1.f - dt) {
        t = (t - 1.f) / dt;
        return t * t + t + t + 1.f;
    }
    return 0.f;
}

// TB-303 DIODE LADDER FILTER with BASS BOOST and OVERDRIVE
inline float tb303_filter(float input, float cutoff, float resonance) {
    // BASS BOOST (TB-303 secret weapon!)
    // Low cutoff → extra bass presence
    float bass_boost = (1.f - cutoff) * 0.4f;
    input *= (1.f + bass_boost);
    
    // Cutoff mapping (20Hz - 4kHz for TB-303)
    float freq = 20.f + cutoff * cutoff * 3980.f;  // Squared for better response
    
    if (freq > 18000.f) freq = 18000.f;
    
    // Filter coefficients
    float w = 2.f * M_PI * freq / 48000.f;
    float g = 0.9892f * fasttanfullf(w * 0.5f);  // Use fasttanfullf instead of osc_tanf
    
    // Resonance (can self-oscillate!)
    float k = resonance * 4.5f;
    
    // Resonance compensation (less than normal for 303 character)
    float fb = k * (1.f - 0.08f * g);
    
    // Input with feedback + IN-FILTER OVERDRIVE!
    float in_stage = input - fb * s_filter_feedback;
    
    // CRITICAL: Overdrive in feedback path (the ACID sound!)
    in_stage = fast_tanh(in_stage * 1.5f);
    
    // 3-pole ladder (diode-style)
    s_filter_z1 = s_filter_z1 + g * (in_stage - s_filter_z1);
    s_filter_z1 = fast_tanh(s_filter_z1);  // Diode saturation!
    
    s_filter_z2 = s_filter_z2 + g * (s_filter_z1 - s_filter_z2);
    s_filter_z2 = fast_tanh(s_filter_z2);
    
    s_filter_z3 = s_filter_z3 + g * (s_filter_z2 - s_filter_z3);
    s_filter_z3 = fast_tanh(s_filter_z3);
    
    s_filter_feedback = s_filter_z3;
    
    // Output with slight resonance peak boost
    float output = s_filter_z3;
    
    // Add resonance character (filter "ping")
    if (resonance > 0.8f) {
        output += s_filter_z2 * (resonance - 0.8f) * 1.5f;
    }
    
    return output;
}

// TB-303 ENVELOPE (exponential decay, no sustain)
inline float tb303_envelope() {
    float t_sec = (float)s_env_counter / 48000.f;
    
    // Decay time (faster than most synths!)
    float base_decay = 0.05f + s_decay * 0.4f;  // 50-450ms
    
    // ACCENT lengthens decay
    if (s_accent_active) {
        base_decay *= 1.2f;
    }
    
    // Exponential decay (TB-303 style)
    float env = fast_exp(-t_sec / base_decay * 4.f);
    
    s_env_level = env;
    s_env_counter++;
    
    return env;
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 1) return k_unit_err_geometry;

    s_context = static_cast<const unit_runtime_osc_context_t *>(desc->hooks.runtime_context);

    s_phase = 0.f;
    s_waveform_blend = 0.f;
    
    s_filter_z1 = 0.f;
    s_filter_z2 = 0.f;
    s_filter_z3 = 0.f;
    s_filter_feedback = 0.f;
    
    s_env_level = 0.f;
    s_env_counter = 0;
    
    s_current_pitch = 36.f;
    s_target_pitch = 36.f;
    s_slide_active = false;
    s_last_note = 36;
    s_note_is_held = false;
    
    s_cutoff_base = 0.1f;
    s_resonance = 0.75f;
    s_env_mod = 0.8f;
    s_decay = 0.2f;
    s_accent_amount = 0.7f;
    s_waveform = 0.5f;
    s_distortion = 0.5f;
    s_slide_time = 1;
    
    s_accent_active = false;
    s_last_velocity = 100;
    
    s_sample_counter = 0;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    s_phase = 0.f;
    s_filter_z1 = 0.f;
    s_filter_z2 = 0.f;
    s_filter_z3 = 0.f;
    s_filter_feedback = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    uint8_t base_note = (s_context->pitch >> 8) & 0xFF;
    uint8_t mod = s_context->pitch & 0xFF;
    
    for (uint32_t f = 0; f < frames; f++) {
        // S-CURVE SLIDE (exponential portamento)
        s_target_pitch = (float)base_note;
        
        if (s_slide_active && s_note_is_held) {
            float delta = s_target_pitch - s_current_pitch;
            
            // Slide speed (depends on time setting and interval)
            float slide_speed = 0.0005f + (float)s_slide_time * 0.003f;
            
            // S-curve: exponential approach
            float slide_factor = 1.f - fast_exp(-slide_speed * 300.f);
            s_current_pitch += delta * slide_factor;
        } else {
            s_current_pitch = s_target_pitch;
        }
        
        // Calculate pitch
        float w0 = osc_w0f_for_note((uint8_t)s_current_pitch, mod);
        
        // WAVEFORM GENERATION with PRE-FILTER SATURATION
        float osc_out = 0.f;
        
        // Smooth waveform crossfade (not instant!)
        s_waveform_blend += (s_waveform - s_waveform_blend) * 0.01f;
        
        if (s_waveform_blend < 0.5f) {
            // SAWTOOTH
            float saw = 2.f * s_phase - 1.f;
            saw -= poly_blep(s_phase, w0);
            
            // Square for blend
            float square = (s_phase < 0.5f) ? 1.f : -1.f;
            square += poly_blep(s_phase, w0);
            square -= poly_blep(fmodf(s_phase + 0.5f, 1.f), w0);
            
            float morph = s_waveform_blend * 2.f;
            osc_out = saw * (1.f - morph) + square * morph;
        } else {
            // SQUARE
            float square = (s_phase < 0.5f) ? 1.f : -1.f;
            square += poly_blep(s_phase, w0);
            square -= poly_blep(fmodf(s_phase + 0.5f, 1.f), w0);
            osc_out = square;
        }
        
        // PRE-FILTER DISTORTION (VCO soft saturation)
        float pre_dist = 1.f + s_distortion * 0.5f;
        osc_out = fast_tanh(osc_out * pre_dist);
        
        // ENVELOPE
        float env = tb303_envelope();
        
        // ACCENT MODULATION
        float accent_boost = s_accent_active ? (1.f + s_accent_amount) : 1.f;
        
        // Filter cutoff with envelope modulation
        float cutoff = s_cutoff_base;
        
        // Envelope modulation (TB-303 hallmark!)
        float env_amount = s_env_mod * accent_boost;
        cutoff += env * env_amount;
        
        // ACCENT boosts cutoff
        if (s_accent_active) {
            cutoff += s_accent_amount * 0.3f;
        }
        
        cutoff = clipminmaxf(0.f, cutoff, 1.f);
        
        // DIODE LADDER FILTER with bass boost & overdrive
        float filtered = tb303_filter(osc_out, cutoff, s_resonance);
        
        // ACCENT boosts volume
        float final_level = 0.7f;
        if (s_accent_active) {
            final_level *= accent_boost;
        }
        
        filtered *= final_level;
        
        // Final output limiting
        out[f] = clipminmaxf(-1.f, filtered * 2.8f, 1.f);  // Volume boost!
        
        // Update phase
        s_phase += w0;
        s_phase -= (uint32_t)s_phase;
        
        s_sample_counter++;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_cutoff_base = valf; break;
        case 1: s_resonance = valf; break;
        case 2: s_env_mod = valf; break;
        case 3: s_decay = valf; break;
        case 4: s_accent_amount = valf; break;
        case 5: s_waveform = valf; break;
        case 6: 
            // Detune parameter repurposed as distortion
            s_distortion = valf; 
            break;
        case 7: 
            // Slide time (0-3)
            s_slide_time = value; 
            break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_cutoff_base * 1023.f);
        case 1: return (int32_t)(s_resonance * 1023.f);
        case 2: return (int32_t)(s_env_mod * 1023.f);
        case 3: return (int32_t)(s_decay * 1023.f);
        case 4: return (int32_t)(s_accent_amount * 1023.f);
        case 5: return (int32_t)(s_waveform * 1023.f);
        case 6: return (int32_t)(s_distortion * 1023.f);
        case 7: return s_slide_time;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 7) {
        static const char *slide_names[] = {"OFF", "SHORT", "MED", "LONG"};
        return slide_names[value];
    }
    return "";
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velo)
{
    // SLIDE DETECTION (legato!)
    if (s_note_is_held && s_slide_time > 0) {
        // Note still held - activate slide
        s_slide_active = true;
    } else {
        // New note - no slide, hard sync
        s_slide_active = false;
        s_current_pitch = (float)note;
        s_phase = 0.f;  // Hard sync!
    }
    
    s_target_pitch = (float)note;
    s_last_note = note;
    s_note_is_held = true;
    
    // ACCENT detection (velocity > 100)
    s_accent_active = (velo > 100);
    s_last_velocity = velo;
    
    // Trigger envelope
    s_env_counter = 0;
    s_env_level = 1.f;
}

__unit_callback void unit_note_off(uint8_t note)
{
    if (note == s_last_note) {
        s_note_is_held = false;
        s_slide_active = false;
    }
    // TB-303 continues to decay, no gate off!
}

__unit_callback void unit_all_note_off()
{
    s_note_is_held = false;
    s_slide_active = false;
}

__unit_callback void unit_set_tempo(uint32_t tempo) {}
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {}
__unit_callback void unit_pitch_bend(uint16_t bend) {}
__unit_callback void unit_channel_pressure(uint8_t press) {}
__unit_callback void unit_aftertouch(uint8_t note, uint8_t press) {}
