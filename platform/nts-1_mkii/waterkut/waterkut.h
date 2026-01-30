/*
    WATERKUT V2 - FIXED VERSION!
    
    Raindrop delay with 10 parallel lines
    Parameters matched to header.c
*/

#pragma once

#include "unit_delfx.h"
#include "fx_api.h"
#include "utils/float_math.h"

class Processor {
public:
    Processor() {}
    ~Processor() {}
    
    static constexpr uint32_t getSampleRate() { return 48000; }
    
    // Core API methods
    void init(float *buffer);
    void teardown();
    void reset();
    void resume();
    void suspend();
    void process(const float *in, float *out, uint32_t frames);
    void setParameter(uint8_t id, int32_t value);
    const char* getParameterStrValue(uint8_t id, int32_t value);
    void setTempo(float bpm);
    void tempo4ppqnTick(uint32_t counter);
    
    // Buffer size: 600k floats = 2.4MB
    static constexpr size_t getBufferSize() {
        return 600000;
    }
    
private:
    // ========== DELAY LINE STRUCTURE ==========
    struct DelayLine {
        float *buffer;
        uint32_t size;
        uint32_t write_pos;
        float base_time;
        float random_offset;
        float feedback_mult;
        float filter_z1;
        float pan;
    };
    
    static constexpr uint8_t NUM_DELAY_LINES = 10;
    DelayLine m_delays[NUM_DELAY_LINES];
    
    // ========== PARAMETERS (MATCHED TO HEADER.C!) ==========
    float m_time;              // Param 0: TIME
    float m_feedback;          // Param 1: DEPTH (feedback)
    float m_mix;               // Param 2: MIX
    float m_chaos;             // Param 3: CHAOS
    float m_mod_intensity;     // Param 4: MOD INTENSITY
    float m_mod_rate;          // Param 5: MOD RATE
    float m_tone;              // Param 6: TONE
    float m_stereo_width;      // Param 7: STEREO WIDTH
    uint8_t m_active_lines;    // Param 8: LINES (1-10)
    float m_diffusion;         // Param 9: DIFFUSION
    bool m_freeze;             // Param 10: FREEZE
    
    // ========== MODULATION ==========
    float m_lfo_phase;
    
    // ========== FILTERS ==========
    float m_tone_z1_l, m_tone_z1_r;
    
    // ========== BUFFER ==========
    float *m_buffer_base;
    size_t m_buffer_allocated;
    
    // ========== RANDOM ==========
    uint32_t m_random_seed;
    
    // ========== HELPER FUNCTIONS ==========
    inline float random_float();
    inline float mod_oscillator();
    inline float process_delay_line(DelayLine &delay, float input);
    inline float read_delay(const DelayLine &delay, float delay_time);
    inline float apply_diffusion(float input);
    void init_random_offsets();
};
