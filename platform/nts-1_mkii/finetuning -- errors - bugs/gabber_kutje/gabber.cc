/*
    GABBER_Kutje - Implementation
    
    "Turn Up The Bass" Edition
    Rhythm Dance Oscillator voor NTS-1 mkII
*/

#include "gabber.h"
#include "utils/int_math.h"
#include <algorithm>
#include <math.h>

// ========== CHORD/INTERVAL RATIOS ==========

// Rave chord intervals (semitones to ratio)
static const float RAVE_INTERVALS[5][8] = {
    // UNISON (all voices same pitch with slight detune)
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
    
    // OCTAVES (voices spread across octaves)
    {0.5f, 0.5f, 1.0f, 1.0f, 2.0f, 2.0f, 4.0f, 4.0f},
    
    // FIFTHS (power chord)
    {1.0f, 1.0f, 1.5f, 1.5f, 2.0f, 2.0f, 3.0f, 3.0f},
    
    // MINOR CHORD (1, m3, 5, octave)
    {1.0f, 1.189f, 1.189f, 1.5f, 1.5f, 2.0f, 2.0f, 2.378f},
    
    // MAJOR CHORD (1, M3, 5, octave)
    {1.0f, 1.260f, 1.260f, 1.5f, 1.5f, 2.0f, 2.0f, 2.520f}
};

// ========== INITIALIZATION ==========

void Gabber::init() {
    // Initialize voices
    for (uint8_t i = 0; i < NUM_VOICES; i++) {
        m_voices[i].phase = 0.f;
        m_voices[i].detune = 0.f;
        m_voices[i].interval = 1.f;
        m_voices[i].pan = (float)i / (float)(NUM_VOICES - 1) * 2.f - 1.f;  // -1 to +1
    }
    
    // Initialize parameters
    m_distortion = 0.5f;
    m_mode = MODE_GABBER;
    m_pitch_env_depth = 0.3f;
    m_sub_level = 0.4f;
    m_pump_depth = 0.5f;
    m_rave_type = RAVE_UNISON;
    
    // Initialize envelopes
    m_pitch_env = 0.f;
    m_amp_env = 0.f;
    m_gate = false;
    m_velocity = 100;
    
    // Initialize pump
    m_pump_phase = 0.f;
    m_pump_env = 1.f;
    
    // Initialize sub
    m_sub_phase = 0.f;
    
    // Initialize PWM
    m_pwm_phase = 0.f;
    m_pwm_width = 0.5f;
    
    // Initialize filter
    m_filter_z1 = 0.f;
}

// ========== NOTE EVENTS ==========

void Gabber::noteOn(uint8_t note, uint8_t velocity) {
    (void)note;  // Note is handled in process()
    
    m_gate = true;
    m_velocity = velocity;
    
    // Reset envelopes
    m_pitch_env = 1.f;  // Start at max for kick punch
    m_amp_env = 1.f;
    
    // Reset pump phase (sync to note)
    m_pump_phase = 0.f;
    
    // Randomize voice detuning
    for (uint8_t i = 0; i < NUM_VOICES; i++) {
        // Small random detune for thickness
        float detune_cents = ((float)(i % 4) - 1.5f) * 5.f;  // ±7.5 cents
        m_voices[i].detune = osc_tanpif(detune_cents / 1200.f);
    }
}

void Gabber::noteOff() {
    m_gate = false;
}

// ========== FAST TANH ==========

inline float Gabber::fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// ========== POLYBLEP (ANTI-ALIASING) ==========

inline float Gabber::polyBLEP(float t, float dt) {
    // PolyBLEP residual for bandlimited synthesis
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.f;
    } else if (t > 1.f - dt) {
        t = (t - 1.f) / dt;
        return t * t + t + t + 1.f;
    }
    return 0.f;
}

// ========== WAVEFORM GENERATION ==========

inline float Gabber::generateWaveform(float phase, float pw) {
    float output = 0.f;
    
    // Ensure phase is normalized [0, 1)
    phase = phase - (int32_t)phase;
    if (phase < 0.f) phase += 1.f;
    
    float dt = 1.f / 48000.f;  // For PolyBLEP
    
    switch (m_mode) {
        case MODE_GABBER: {
            // Distorted triangle/sine hybrid
            float tri = (phase < 0.5f) ? (4.f * phase - 1.f) : (3.f - 4.f * phase);
            float sine = osc_sinf(phase);
            output = tri * 0.7f + sine * 0.3f;
            break;
        }
        
        case MODE_NUSTYLE: {
            // Complex pitch-swept kick (saw + sine)
            float saw = 2.f * phase - 1.f;
            saw -= polyBLEP(phase, dt);
            float kick_sine = osc_sinf(phase * (1.f + m_pitch_env * 3.f));
            output = saw * 0.4f + kick_sine * 0.6f;
            break;
        }
        
        case MODE_EUROHOUSE: {
            // Organ-like square (with harmonics)
            float square = (phase < pw) ? 1.f : -1.f;
            square += polyBLEP(phase, dt);
            square -= polyBLEP(fmodf(phase + (1.f - pw), 1.f), dt);
            output = square;
            break;
        }
        
        case MODE_HARDTECH: {
            // Noisy rumble (saw + noise-like harmonics)
            float saw = 2.f * phase - 1.f;
            saw -= polyBLEP(phase, dt);
            float noise = osc_sinf(phase * 7.f) * 0.3f;  // Harmonic content
            output = saw * 0.7f + noise * 0.3f;
            break;
        }
        
        case MODE_RAVESTAB: {
            // Sharp sawtooth
            float saw = 2.f * phase - 1.f;
            saw -= polyBLEP(phase, dt);
            output = saw;
            break;
        }
        
        case MODE_DANCEPAD: {
            // Rich PWM with slow LFO
            float square = (phase < m_pwm_width) ? 1.f : -1.f;
            square += polyBLEP(phase, dt);
            square -= polyBLEP(fmodf(phase + (1.f - m_pwm_width), 1.f), dt);
            output = square;
            break;
        }
        
        case MODE_TERRORSAW: {
            // Metallic saw with high harmonics
            float saw = 2.f * phase - 1.f;
            saw -= polyBLEP(phase, dt);
            float metallic = osc_sinf(phase * 11.f) * 0.4f;
            output = saw * 0.6f + metallic * 0.4f;
            break;
        }
        
        case MODE_HOOVER: {
            // Classic hoover (PWM + detuned saws)
            float square = (phase < 0.4f) ? 1.f : -1.f;
            square += polyBLEP(phase, dt);
            square -= polyBLEP(fmodf(phase + 0.6f, 1.f), dt);
            
            float saw = 2.f * phase - 1.f;
            saw -= polyBLEP(phase, dt);
            
            output = square * 0.5f + saw * 0.5f;
            break;
        }
    }
    
    return output;
}

// ========== DISTORTION ==========

inline float Gabber::distortionCurve(float x, float amount) {
    if (amount < 0.01f) return x;
    
    // Gabber-style distortion (preserve bass, destroy highs)
    // Soft clip → hard clip transition
    
    float drive = 1.f + amount * 19.f;  // 1x to 20x
    x *= drive;
    
    // Wavefolder-style distortion
    if (amount > 0.7f) {
        // Hard clipping + foldback
        while (x > 1.f) x = 2.f - x;
        while (x < -1.f) x = -2.f - x;
    } else {
        // Soft saturation
        x = fast_tanh(x);
    }
    
    return x;
}

// ========== RAVE INTERVAL CALCULATION ==========

inline float Gabber::getRaveInterval(uint8_t voice_idx) {
    return RAVE_INTERVALS[m_rave_type][voice_idx];
}

// ========== ENVELOPE UPDATES ==========

inline void Gabber::updateEnvelopes() {
    // Pitch envelope (exponential decay)
    if (m_gate) {
        float pitch_decay = 0.9995f;  // Fast decay for kick punch
        m_pitch_env *= pitch_decay;
        if (m_pitch_env < 0.001f) m_pitch_env = 0.f;
    } else {
        m_pitch_env = 0.f;
    }
    
    // Amplitude envelope (simple AR)
    if (m_gate) {
        float amp_decay = 0.9998f;
        m_amp_env *= amp_decay;
    } else {
        float release = 0.999f;
        m_amp_env *= release;
    }
    
    // Clamp
    if (m_amp_env < 0.001f) m_amp_env = 0.f;
}

// ========== PUMP UPDATE (4/4 SIDECHAIN SIMULATION) ==========

inline void Gabber::updatePump() {
    // Simulate 4/4 kick ducking at 128 BPM
    const float BPM = 128.f;
    const float beat_freq = BPM / 60.f;  // Beats per second
    
    // Update pump phase
    m_pump_phase += beat_freq / 48000.f;
    if (m_pump_phase >= 1.f) m_pump_phase -= 1.f;
    
    // Generate pump envelope (kick on beat, swell between)
    // Sharp attack, slow release
    if (m_pump_phase < 0.05f) {
        // Attack (0-50ms)
        m_pump_env = m_pump_phase / 0.05f;
    } else {
        // Release
        float release_phase = (m_pump_phase - 0.05f) / 0.95f;
        m_pump_env = 1.f - release_phase * 0.6f;  // Duck by 60% max
    }
    
    // Apply pump depth
    m_pump_env = 1.f - (1.f - m_pump_env) * m_pump_depth;
    
    // Smooth envelope
    m_pump_env = clipminmaxf(0.f, m_pump_env, 1.f);
}

// ========== MAIN PROCESSING ==========

void Gabber::process(const float w0_base, const uint8_t note, const uint8_t mod, 
                     q31_t *yn, const uint32_t frames) {
    (void)note;
    (void)mod;
    
    // Calculate base frequency with pitch envelope
    // w0_base is normalized frequency (phase increment per sample)
    float pitch_mod = 1.f + m_pitch_env * m_pitch_env_depth * 3.f;  // Up to +3 octaves
    float w0 = w0_base * pitch_mod;
    
    // Update PWM LFO (for dance pad mode)
    const float pwm_rate = 0.5f;  // 0.5 Hz
    m_pwm_phase += pwm_rate / 48000.f;
    if (m_pwm_phase >= 1.f) m_pwm_phase -= 1.f;
    m_pwm_width = 0.3f + osc_sinf(m_pwm_phase) * 0.2f;  // 0.1 to 0.5
    
    q31_t * __restrict y = yn;
    
    for (uint32_t i = 0; i < frames; i++) {
        // Update envelopes per sample
        updateEnvelopes();
        updatePump();
        
        // ========== ENSEMBLE PROCESSING (8 VOICES) ==========
        
        float ensemble = 0.f;
        
        for (uint8_t v = 0; v < NUM_VOICES; v++) {
            // Calculate voice frequency
            float interval = getRaveInterval(v);
            float detune = 1.f + m_voices[v].detune * 0.01f;  // ±1%
            float voice_w0 = w0 * interval * detune;
            
            // Generate waveform
            float voice_out = generateWaveform(m_voices[v].phase, m_pwm_width);
            
            // Apply voice amplitude (spread across stereo field)
            float voice_level = 1.f / (float)NUM_VOICES;
            
            // Stereo panning (not used in mono output, but keeps structure)
            ensemble += voice_out * voice_level;
            
            // Advance phase
            m_voices[v].phase += voice_w0;
            if (m_voices[v].phase >= 1.f) m_voices[v].phase -= 1.f;
        }
        
        // ========== SUB OSCILLATOR ==========
        
        float sub_out = 0.f;
        if (m_sub_level > 0.01f) {
            // Clean sine wave -1 octave
            sub_out = osc_sinf(m_sub_phase) * m_sub_level;
            
            float sub_w0 = w0 * 0.5f;  // -1 octave
            m_sub_phase += sub_w0;
            if (m_sub_phase >= 1.f) m_sub_phase -= 1.f;
        }
        
        // ========== MIX ==========
        
        float mixed = ensemble + sub_out;
        
        // ========== DISTORTION ==========
        
        mixed = distortionCurve(mixed, m_distortion);
        
        // ========== SIMPLE LOWPASS FILTER ==========
        
        // One-pole lowpass to tame harshness
        float cutoff = 0.3f;  // Relatively open
        m_filter_z1 = m_filter_z1 + cutoff * (mixed - m_filter_z1);
        mixed = m_filter_z1;
        
        // ========== APPLY ENVELOPES ==========
        
        // Amplitude envelope
        mixed *= m_amp_env;
        
        // Velocity sensitivity
        mixed *= (float)m_velocity / 127.f;
        
        // Apply pump (sidechain)
        mixed *= m_pump_env;
        
        // ========== FINAL LIMITING ==========
        
        mixed = clipminmaxf(-1.f, mixed, 1.f);
        
        // ========== OUTPUT (CONVERT TO Q31) ==========
        
        y[i] = f32_to_q31(mixed);
    }
}

// ========== PARAMETER UPDATES ==========

void Gabber::setParameter(uint8_t id, int32_t value) {
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: // DIST (Distortion)
            m_distortion = valf;
            break;
            
        case 1: // MODE (Genre selector)
            m_mode = (Mode)clipminmaxi32(0, value, 7);
            break;
            
        case 2: // PENV (Pitch envelope depth)
            m_pitch_env_depth = valf;
            break;
            
        case 3: // SUB (Sub oscillator level)
            m_sub_level = valf;
            break;
            
        case 4: // PUMP (Sidechain depth)
            m_pump_depth = valf;
            break;
            
        case 5: // RAVE (Chord type)
            m_rave_type = (RaveType)clipminmaxi32(0, value, 4);
            break;
            
        default:
            break;
    }
}

const char* Gabber::getParameterStrValue(uint8_t id, int32_t value) {
    if (id == 1) {  // MODE
        static const char *mode_names[] = {
            "GABBER", "NUSTYLE", "EUROHAUS", "HARDTECH",
            "RAVESTAB", "DANCEPAD", "TERRORSAW", "HOOVER"
        };
        if (value >= 0 && value < 8) return mode_names[value];
    }
    
    if (id == 5) {  // RAVE
        static const char *rave_names[] = {
            "UNISON", "OCTAVES", "FIFTHS", "MINOR", "MAJOR"
        };
        if (value >= 0 && value < 5) return rave_names[value];
    }
    
    return "";
}

