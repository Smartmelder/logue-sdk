/*
    GABBER_Kutje - Rhythm Dance Oscillator Header
    
    8-Voice Ensemble Engine for:
    - Early Hardcore Gabber
    - Eurohouse
    - Hardtechno
    - Rave Classics
*/

#pragma once

#include "unit_osc.h"
#include "osc_api.h"
#include "utils/float_math.h"

class Gabber {
public:
    Gabber() {}
    ~Gabber() {}
    
    static constexpr uint32_t getSampleRate() { return 48000; }
    
    // Core API
    void init();
    void process(const float w0_base, const uint8_t note, const uint8_t mod, q31_t *yn, const uint32_t frames);
    void noteOn(uint8_t note, uint8_t velocity);
    void noteOff();
    void setParameter(uint8_t id, int32_t value);
    const char* getParameterStrValue(uint8_t id, int32_t value);
    
private:
    // ========== MODES ==========
    enum Mode {
        MODE_GABBER = 0,      // Early Gabber (distorted sine/tri)
        MODE_NUSTYLE = 1,     // Nu-Style kick (pitch sweep)
        MODE_EUROHOUSE = 2,   // Eurohouse bass (organ square)
        MODE_HARDTECH = 3,    // Hardtechno rumble
        MODE_RAVESTAB = 4,    // Rave stab (saw)
        MODE_DANCEPAD = 5,    // Dance pad (PWM)
        MODE_TERRORSAW = 6,   // Terror saw (metallic)
        MODE_HOOVER = 7       // Classic hoover
    };
    
    // ========== RAVE CHORD TYPES ==========
    enum RaveType {
        RAVE_UNISON = 0,
        RAVE_OCTAVES = 1,
        RAVE_FIFTHS = 2,
        RAVE_MINOR = 3,
        RAVE_MAJOR = 4
    };
    
    // ========== VOICE STRUCTURE ==========
    struct Voice {
        float phase;
        float detune;
        float interval;  // For chord spreading
        float pan;       // Stereo position
    };
    
    static constexpr uint8_t NUM_VOICES = 8;
    Voice m_voices[NUM_VOICES];
    
    // ========== PARAMETERS ==========
    float m_distortion;      // 0-1
    Mode m_mode;             // 0-7
    float m_pitch_env_depth; // Pitch envelope depth
    float m_sub_level;       // Sub oscillator level
    float m_pump_depth;       // Sidechain pump depth
    RaveType m_rave_type;    // Chord type
    
    // ========== ENVELOPES ==========
    float m_pitch_env;       // Current pitch envelope value
    float m_amp_env;         // Amplitude envelope
    bool m_gate;             // Note gate
    uint8_t m_velocity;      // MIDI velocity
    
    // ========== PUMP (SIDECHAIN) ==========
    float m_pump_phase;      // 4/4 beat simulation
    float m_pump_env;       // Current pump envelope
    
    // ========== SUB OSCILLATOR ==========
    float m_sub_phase;       // Sub osc phase (-1 octave)
    
    // ========== PWM (for dance pad mode) ==========
    float m_pwm_phase;       // PWM LFO
    float m_pwm_width;       // Current pulse width
    
    // ========== FILTER (simple one-pole) ==========
    float m_filter_z1;
    
    // ========== HELPERS ==========
    inline float polyBLEP(float t, float dt);
    inline float generateWaveform(float phase, float pw);
    inline float distortionCurve(float x, float amount);
    inline void updateEnvelopes();
    inline void updatePump();
    inline float getRaveInterval(uint8_t voice_idx);
    inline float fast_tanh(float x);
};

