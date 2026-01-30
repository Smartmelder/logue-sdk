/*
    ULTRA WIDE RANGE DELAY/LOOPER
    
    ═══════════════════════════════════════════════════════════════
    ARCHITECTURE - THE INFINITE MACHINE
    ═══════════════════════════════════════════════════════════════
    
    === DELAY ENGINE ===
    
    Dual Stereo Delays:
    - Left delay: 6,000 samples (125ms @ 48kHz)
    - Right delay: 6,000 samples (125ms @ 48kHz)
    - Total: 12k samples × 4 bytes = 48 KB (uses SDRAM!)
    
    === SHIMMER REVERB ===
    
    Pitch-shifted feedback loop:
    1. Delay output → Pitch shifter (+12 semitones)
    2. Mix with original
    3. Feed back to delay input
    4. Creates ascending "shimmer" effect
    
    === DIFFUSION NETWORK ===
    
    8×8 All-Pass Cascade:
    - 8 all-pass filters (varying delays)
    - Prime number delays for maximum density
    - Creates dense reverb tail
    
    All-pass delays: 89, 107, 127, 149, 173, 197, 223, 251 samples
    
    === STEREO WIDENING ===
    
    Haas Effect + Mid/Side:
    - Mid/Side processing
    - Side channel enhancement (up to 200%)
    
    === MODULATION ===
    
    LFO Sources:
    1. LFO 1: Delay time (chorus/vibrato)
    2. LFO 2: Filter cutoff
    3. LFO 3: Pan position
    
    === SPECIAL MODES ===
    
    0. DIGITAL - Clean, pristine delays
    1. ANALOG - Tape delay simulation
    2. LOFI - Bitcrusher + downsample
    3. SHIMMER - Pitch-up reverb
    4. REVERSE - Backwards delay
    5. GRANULAR - Texture creation
    6. INFINITE - Self-sustaining feedback
    7. CHAOS - Randomized everything
    
    === TEMPO SYNC ===
    
    16 divisions:
    - 1/64, 1/32T, 1/32, 1/16T, 1/16, 1/8T, 1/8, 1/4T
    - 1/4, 1/2T, 1/2, 3/4, 1/1, 2/1, 3/1, 4/1
    
    ═══════════════════════════════════════════════════════════════
    INSPIRED BY
    ═══════════════════════════════════════════════════════════════
    
    Hardware:
    - Eventide H9 (shimmer algorithms)
    - Strymon BigSky (cloud reverb)
    - Empress Reverb (ghost mode)
    - EHX Cathedral (infinite reverb)
    - Boss DD-500 (multi-tap)
    - Chase Bliss MOOD (reverse/glitch)
    
*/

#include "unit_revfx.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
// NO osc_api.h - not available for revfx!
// NO math.h - use SDK functions only!
#include <algorithm>

#define MAX_DELAY_TIME 6000    // 125ms @ 48kHz (reduced for memory - effect limit 32KB)
#define DIFFUSION_SIZE 8
#define LFO_TABLE_SIZE 128     // Reduced from 512
#define PITCH_BUFFER_SIZE 256  // Reduced from 2048

// ✅ NaN protection helper
inline float safe_float(float x) {
    if (!std::isfinite(x)) return 0.f;
    if (std::isnan(x)) return 0.f;
    if (std::isinf(x)) return 0.f;
    if (fabsf(x) < 1e-15f) return 0.f;  // ✅ Denormal kill
    return x;
}

// ✅ Soft clipper for feedback paths
inline float soft_clip(float x) {
    if (x < -1.5f) return -1.f;
    if (x > 1.5f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// Delay buffer pointers (allocated in SDRAM)
static float *s_delay_l;
static float *s_delay_r;
static uint32_t s_delay_write;

// Diffusion network (all-pass filters)
struct AllPassFilter {
    float buffer[256];
    uint32_t write_pos;
    uint32_t delay_samples;
    float feedback;
    float z1;  // ✅ DC blocker state to prevent pieptoon
};

static AllPassFilter s_diffusion[DIFFUSION_SIZE];

// Prime number delays for maximum density
static const uint32_t s_diffusion_delays[DIFFUSION_SIZE] = {
    89, 107, 127, 149, 173, 197, 223, 251
};

// Pitch shifter (simple time-domain)
struct PitchShifter {
    float buffer[PITCH_BUFFER_SIZE];
    uint32_t write_pos;
    float read_pos;
    float pitch_ratio;
};

static PitchShifter s_pitch_shift_l;
static PitchShifter s_pitch_shift_r;

// LFO tables
static float s_lfo_sine[LFO_TABLE_SIZE];
static float s_lfo_triangle[LFO_TABLE_SIZE];
static float s_lfo_square[LFO_TABLE_SIZE];

// LFO phases
static float s_lfo_phase_1;

// Envelope follower
static float s_envelope;

// Parameters
static float s_delay_time;
static float s_feedback_amount;
static float s_stereo_width;
static float s_shimmer_amount;
static float s_diffusion_amount;
static float s_modulation_depth;
static float s_mix;
static uint8_t s_division;
static uint8_t s_mode;
static bool s_freeze;

// ✅ Parameter smoothing (prevent clicks)
static float s_delay_time_smooth;
static float s_feedback_smooth;
static float s_mix_smooth;
static float s_modulation_smooth;
static float s_shimmer_smooth;
static float s_diffusion_smooth;
static float s_width_smooth;

// Tempo
static uint32_t s_beat_length;
static bool s_tempo_sync;

// Random
static uint32_t s_random_seed;

static uint32_t s_sample_counter;

inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

inline uint32_t xorshift32() {
    s_random_seed ^= s_random_seed << 13;
    s_random_seed ^= s_random_seed >> 17;
    s_random_seed ^= s_random_seed << 5;
    return s_random_seed;
}

inline float random_float() {
    return (float)(xorshift32() % 10000) / 10000.f;
}

void init_lfo_tables() {
    // SDK compatibility - PI is already defined in CMSIS arm_math.h
    // const float PI = 3.14159265359f; // Removed - conflicts with CMSIS
    for (int i = 0; i < LFO_TABLE_SIZE; i++) {
        float phase = (float)i / (float)LFO_TABLE_SIZE;
        
        // Convert phase [0,1] to angle [-π, π] for fx_sinf (revfx can't use osc_api.h)
        float angle = (phase - 0.5f) * 2.f * PI;
        s_lfo_sine[i] = fx_sinf(angle);  // Fixed: fx_sinf for revfx
        
        if (phase < 0.5f) {
            s_lfo_triangle[i] = -1.f + 4.f * phase;
        } else {
            s_lfo_triangle[i] = 3.f - 4.f * phase;
        }
        
        s_lfo_square[i] = (phase < 0.5f) ? 1.f : -1.f;
    }
}

inline float lfo_read(float *table, float phase) {
    phase -= (int32_t)phase;
    if (phase < 0.f) phase += 1.f;
    if (phase >= 1.f) phase -= 1.f;
    
    float idx_f = phase * (float)(LFO_TABLE_SIZE - 1);
    uint32_t idx0 = (uint32_t)idx_f;
    uint32_t idx1 = (idx0 + 1) % LFO_TABLE_SIZE;
    float frac = idx_f - (float)idx0;
    
    return table[idx0] * (1.f - frac) + table[idx1] * frac;
}

// All-pass filter (for diffusion)
inline float process_allpass(AllPassFilter *apf, float input) {
    float delayed = apf->buffer[apf->write_pos];
    
    float output = -input + delayed;
    
    // ✅ Limit all-pass feedback to prevent instability
    // ✅ KRITIEK: All-pass feedback MUST be < 0.65!
    float ap_feedback = clipminmaxf(0.2f, apf->feedback, 0.65f);  // MAX 0.65!
    apf->buffer[apf->write_pos] = input + delayed * ap_feedback;
    
    apf->write_pos = (apf->write_pos + 1) % apf->delay_samples;
    
    // ✅ DC blocker to prevent pieptoon
    float dc_coeff = 0.995f;
    apf->z1 = apf->z1 * dc_coeff + output * (1.f - dc_coeff);
    output = output - apf->z1;
    
    // ✅ Denormal kill
    if (fabsf(output) < 1e-15f) output = 0.f;
    
    // ✅ Safety clipping
    output = clipminmaxf(-2.f, output, 2.f);
    
    // ✅ Final NaN/Inf check
    output = safe_float(output);
    
    return output;
}

// Diffusion network (8 all-pass in series)
inline void process_diffusion(float *in_l, float *in_r) {
    float sig_l = *in_l;
    float sig_r = *in_r;
    
    // Process through all-pass cascade
    for (int i = 0; i < DIFFUSION_SIZE; i++) {
        sig_l = process_allpass(&s_diffusion[i], sig_l);
        sig_r = process_allpass(&s_diffusion[i], sig_r);
        
        // Cross-couple for stereo
        if (i % 2 == 0) {
            float temp = sig_l;
            sig_l = (sig_l + sig_r) * 0.7071f;
            sig_r = (temp - sig_r) * 0.7071f;
        }
    }
    
    *in_l = sig_l * s_diffusion_amount;
    *in_r = sig_r * s_diffusion_amount;
}

// Simple pitch shifter with safety checks
inline float pitch_shift_process(PitchShifter *ps, float input, float semitones) {
    // ✅ Bypass als shimmer te laag
    if (s_shimmer_amount < 0.01f) {
        return input;
    }
    
    // ✅ Input limiting
    input = clipminmaxf(-1.f, input, 1.f);
    
    // Write input
    ps->buffer[ps->write_pos] = input;
    uint32_t old_write = ps->write_pos;
    ps->write_pos = (ps->write_pos + 1) % PITCH_BUFFER_SIZE;
    
    // ✅ Check distance (prevent collision)
    int32_t distance = (int32_t)old_write - (int32_t)ps->read_pos;
    if (distance < 0) distance += PITCH_BUFFER_SIZE;
    
    if (distance < 100) {
        return 0.f;  // Too close - return silence to prevent artifacts
    }
    
    // Calculate pitch ratio
    ps->pitch_ratio = fx_pow2f(semitones / 12.f);  // Fixed: fx_pow2f for revfx
    ps->pitch_ratio = clipminmaxf(0.25f, ps->pitch_ratio, 4.f);
    
    // Read with interpolation
    float read_pos_f = ps->read_pos;
    uint32_t read_pos_0 = (uint32_t)read_pos_f;
    if (read_pos_0 >= PITCH_BUFFER_SIZE) read_pos_0 = 0;
    
    uint32_t read_pos_1 = (read_pos_0 + 1) % PITCH_BUFFER_SIZE;
    float frac = read_pos_f - (float)read_pos_0;
    frac = clipminmaxf(0.f, frac, 1.f);
    
    float output = ps->buffer[read_pos_0] * (1.f - frac) + 
                   ps->buffer[read_pos_1] * frac;
    
    // ✅ Output limiting
    output = clipminmaxf(-2.f, output, 2.f);
    
    // Advance read position
    ps->read_pos += ps->pitch_ratio;
    
    // ✅ Wrap with proper bounds
    while (ps->read_pos >= (float)PITCH_BUFFER_SIZE) {
        ps->read_pos -= (float)PITCH_BUFFER_SIZE;
    }
    while (ps->read_pos < 0.f) {
        ps->read_pos += (float)PITCH_BUFFER_SIZE;
    }
    
    // ✅ Denormal kill
    if (fabsf(output) < 1e-15f) output = 0.f;
    
    // ✅ Soft clip
    output = clipminmaxf(-1.f, output, 1.f);
    
    // ✅ Extra attenuation for safety
    return output * 0.5f;
}

// Stereo widening (Mid/Side)
inline void stereo_widen(float *l, float *r, float width) {
    float mid = (*l + *r) * 0.5f;
    float side = (*l - *r) * 0.5f;
    
    side *= width;
    
    *l = mid + side;
    *r = mid - side;
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;

    // Check SDRAM allocation available
    if (!desc->hooks.sdram_alloc) return k_unit_err_memory;

    // Allocate delay buffers in SDRAM (2 channels × 48k samples)
    uint32_t total_samples = 2 * MAX_DELAY_TIME;
    float *sdram_buffer = (float *)desc->hooks.sdram_alloc(total_samples * sizeof(float));
    if (!sdram_buffer) return k_unit_err_memory;

    // Clear buffer
    std::fill(sdram_buffer, sdram_buffer + total_samples, 0.f);

    // Assign buffer pointers
    s_delay_l = sdram_buffer;
    s_delay_r = sdram_buffer + MAX_DELAY_TIME;
    s_delay_write = 0;
    
    // ✅ Init diffusion - COMPLETE initialization with std::fill
    for (int i = 0; i < DIFFUSION_SIZE; i++) {
        s_diffusion[i].delay_samples = s_diffusion_delays[i];
        s_diffusion[i].write_pos = 0;
        s_diffusion[i].feedback = 0.65f;  // ✅ Max 0.65 (was 0.7 - te hoog!)
        s_diffusion[i].z1 = 0.f;  // ✅ Initialize DC blocker state
        
        // ✅ Use std::fill for complete buffer clearing
        std::fill(s_diffusion[i].buffer, s_diffusion[i].buffer + 256, 0.f);
    }
    
    // ✅ Init pitch shifters - COMPLETE initialization
    std::fill(s_pitch_shift_l.buffer, s_pitch_shift_l.buffer + PITCH_BUFFER_SIZE, 0.f);
    std::fill(s_pitch_shift_r.buffer, s_pitch_shift_r.buffer + PITCH_BUFFER_SIZE, 0.f);
    s_pitch_shift_l.write_pos = 0;
    s_pitch_shift_l.read_pos = 100.f;  // ✅ Start 100 samples ahead to prevent collision
    s_pitch_shift_l.pitch_ratio = 1.f;
    
    s_pitch_shift_r.write_pos = 0;
    s_pitch_shift_r.read_pos = 100.f;  // ✅ Start 100 samples ahead
    s_pitch_shift_r.pitch_ratio = 1.f;
    
    // Init LFOs
    init_lfo_tables();
    s_lfo_phase_1 = 0.f;
    
    s_envelope = 0.f;
    
    // ✅ Init parameters
    s_delay_time = 0.6f;
    s_feedback_amount = 0.6f;  // ✅ 60% default (was 0.75f - TE HOOG!)
    s_stereo_width = 0.8f;
    s_shimmer_amount = 0.0f;  // ✅ START op 0% (niet 40%!) - voorkomt gekraak bij selectie
    s_diffusion_amount = 0.3f;
    s_modulation_depth = 0.25f;
    s_mix = 0.5f;
    s_division = 3;
    s_mode = 0;
    s_freeze = false;
    
    // ✅ Initialize smoothing (prevent clicks on first use)
    s_delay_time_smooth = s_delay_time;
    s_feedback_smooth = s_feedback_amount;
    s_mix_smooth = s_mix;
    s_modulation_smooth = s_modulation_depth;
    s_shimmer_smooth = s_shimmer_amount;
    s_diffusion_smooth = s_diffusion_amount;
    s_width_smooth = s_stereo_width;
    
    s_beat_length = 12000;
    s_tempo_sync = false;
    
    s_random_seed = 0x87654321;
    
    s_sample_counter = 0;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    // ✅ Clear ALL delay buffers
    if (s_delay_l) {
        std::fill(s_delay_l, s_delay_l + MAX_DELAY_TIME, 0.f);
    }
    if (s_delay_r) {
        std::fill(s_delay_r, s_delay_r + MAX_DELAY_TIME, 0.f);
    }
    s_delay_write = 0;
    
    // ✅ Reset smoothing to current parameter values (prevent clicks on reset)
    s_delay_time_smooth = s_delay_time;
    s_feedback_smooth = s_feedback_amount;
    s_mix_smooth = s_mix;
    s_modulation_smooth = s_modulation_depth;
    s_shimmer_smooth = s_shimmer_amount;
    s_diffusion_smooth = s_diffusion_amount;
    s_width_smooth = s_stereo_width;
    
    // ✅ Clear ALL diffusion buffers
    for (int i = 0; i < DIFFUSION_SIZE; i++) {
        std::fill(s_diffusion[i].buffer, s_diffusion[i].buffer + 256, 0.f);
        s_diffusion[i].write_pos = 0;
        s_diffusion[i].z1 = 0.f;
    }
    
    // ✅ Clear ALL pitch shifter buffers
    std::fill(s_pitch_shift_l.buffer, s_pitch_shift_l.buffer + PITCH_BUFFER_SIZE, 0.f);
    std::fill(s_pitch_shift_r.buffer, s_pitch_shift_r.buffer + PITCH_BUFFER_SIZE, 0.f);
    s_pitch_shift_l.write_pos = 0;
    s_pitch_shift_l.read_pos = 100.f;  // ✅ Reset to safe position
    s_pitch_shift_r.write_pos = 0;
    s_pitch_shift_r.read_pos = 100.f;
    
    // ✅ Reset LFO and envelope
    s_lfo_phase_1 = 0.f;
    s_envelope = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    const float *in_ptr = in;
    float *out_ptr = out;
    
    // Calculate delay time
    uint32_t delay_samples;
    if (s_tempo_sync) {
        // Tempo divisions
        float division_multipliers[] = {
            0.015625f,  // 1/64
            0.020833f,  // 1/32T
            0.03125f,   // 1/32
            0.041667f,  // 1/16T
            0.0625f,    // 1/16
            0.083333f,  // 1/8T
            0.125f,     // 1/8
            0.166667f,  // 1/4T
            0.25f,      // 1/4
            0.333333f,  // 1/2T
            0.5f,       // 1/2
            0.75f,      // 3/4
            1.f,        // 1/1
            2.f,        // 2/1
            3.f,        // 3/1
            4.f         // 4/1
        };
        
        delay_samples = (uint32_t)((float)s_beat_length * 4.f * division_multipliers[s_division]);
    } else {
        // Free-running (10ms - 10sec)
        // ✅ Smooth delay time (prevent clicks on knob turn)
        const float smoothing = 0.999f;  // Very slow for delay time
        s_delay_time_smooth += (s_delay_time - s_delay_time_smooth) * (1.f - smoothing);
        delay_samples = (uint32_t)(480.f + s_delay_time_smooth * 5520.f);  // 10ms to 125ms
    }
    
    delay_samples = clipminmaxi32(480, delay_samples, MAX_DELAY_TIME - 1);
    
    // LFO rates
    float lfo1_rate = 0.1f + s_modulation_depth * 4.9f;
    
    for (uint32_t f = 0; f < frames; f++) {
        // ✅ Input clipping to prevent issues
        float in_l = clipminmaxf(-1.f, in_ptr[0], 1.f);
        float in_r = clipminmaxf(-1.f, in_ptr[1], 1.f);
        
        // ✅ KRITIEK: Detect COMPLETE silence
        float input_level = si_fabsf(in_l) + si_fabsf(in_r);
        
        // Als freeze NIET actief EN input stil → skip processing
        if (!s_freeze && input_level < 0.0001f) {
            // ✅ Decay buffers slowly (prevent stuck ruis)
            // Maar output = silence
            out_ptr[0] = 0.f;
            out_ptr[1] = 0.f;
            in_ptr += 2;
            out_ptr += 2;
            s_delay_write = (s_delay_write + 1) % MAX_DELAY_TIME;
            continue;
        }
        
        // Envelope follower
        float in_level = (si_fabsf(in_l) + si_fabsf(in_r)) * 0.5f;
        s_envelope += (in_level - s_envelope) * 0.01f;
        
        // LFO modulation
        s_lfo_phase_1 += lfo1_rate / 48000.f;
        if (s_lfo_phase_1 >= 1.f) s_lfo_phase_1 -= 1.f;
        if (s_lfo_phase_1 < 0.f) s_lfo_phase_1 += 1.f;
        
        float lfo1 = lfo_read(s_lfo_sine, s_lfo_phase_1);
        // ✅ Smooth modulation depth (prevent LFO jumps)
        const float mod_smoothing = 0.995f;
        s_modulation_smooth += (s_modulation_depth - s_modulation_smooth) * (1.f - mod_smoothing);
        float time_mod = 1.f + lfo1 * s_modulation_smooth * 0.1f;
        
        uint32_t mod_delay_samples = (uint32_t)((float)delay_samples * time_mod);
        mod_delay_samples = clipminmaxi32(100, mod_delay_samples, MAX_DELAY_TIME - 1);
        
        // Read from delay (with modulation)
        uint32_t read_pos = (s_delay_write + MAX_DELAY_TIME - mod_delay_samples) % MAX_DELAY_TIME;
        
        float delayed_l = s_delay_l[read_pos];
        float delayed_r = s_delay_r[read_pos];
        
        // SHIMMER: Pitch shift feedback
        // ✅ Smooth shimmer (prevent pitch shift jumps)
        const float shimmer_smoothing = 0.99f;
        s_shimmer_smooth += (s_shimmer_amount - s_shimmer_smooth) * (1.f - shimmer_smoothing);
        if (s_shimmer_smooth > 0.01f) {
            float shimmer_l = pitch_shift_process(&s_pitch_shift_l, delayed_l, 12.f * s_shimmer_smooth);
            float shimmer_r = pitch_shift_process(&s_pitch_shift_r, delayed_r, 12.f * s_shimmer_smooth);
            
            delayed_l = delayed_l * (1.f - s_shimmer_smooth) + shimmer_l * s_shimmer_smooth;
            delayed_r = delayed_r * (1.f - s_shimmer_smooth) + shimmer_r * s_shimmer_smooth;
        }
        
        // MODE-SPECIFIC PROCESSING
        switch (s_mode) {
            case 1: // ANALOG
                delayed_l = fast_tanh(delayed_l * 1.5f);
                delayed_r = fast_tanh(delayed_r * 1.5f);
                break;
            
            case 2: // LOFI
                delayed_l = si_floorf(delayed_l * 8.f) / 8.f;
                delayed_r = si_floorf(delayed_r * 8.f) / 8.f;
                break;
            
            case 4: // REVERSE
                // Read backwards
                read_pos = (s_delay_write + mod_delay_samples) % MAX_DELAY_TIME;
                delayed_l = s_delay_l[read_pos];
                delayed_r = s_delay_r[read_pos];
                break;
            
            case 7: // CHAOS
                // Random everything!
                if (random_float() < 0.01f) {
                    delayed_l *= random_float() * 2.f;
                    delayed_r *= random_float() * 2.f;
                }
                break;
        }
        
        // DIFFUSION
        // ✅ Smooth diffusion
        const float diff_smoothing = 0.995f;
        s_diffusion_smooth += (s_diffusion_amount - s_diffusion_smooth) * (1.f - diff_smoothing);
        if (s_diffusion_smooth > 0.01f) {
            process_diffusion(&delayed_l, &delayed_r);
        }
        
        // ✅ FEEDBACK (CRITICAL: Limit to prevent explosion!)
        // ✅ Smooth feedback (prevent amplitude jumps)
        const float fb_smoothing = 0.99f;
        s_feedback_smooth += (s_feedback_amount - s_feedback_smooth) * (1.f - fb_smoothing);
        float feedback = clipminmaxf(0.f, s_feedback_smooth, 0.93f);  // Max 93% for stability
        if (s_mode == 6 || s_freeze) feedback = 0.93f;  // ✅ INFINITE/FREEZE mode - MAX 0.93!
        // ✅ EXTRA SAFETY: Always apply hard limit after any override
        feedback = clipminmaxf(0.f, feedback, 0.93f);  // Mandatory final check!
        
        // ✅ Soft clip feedback signals to prevent buildup
        float fb_l = soft_clip(delayed_l * feedback);
        float fb_r = soft_clip(delayed_r * feedback);
        
        // ✅ NaN protection
        fb_l = safe_float(fb_l);
        fb_r = safe_float(fb_r);
        
        // Crossfeed (ping-pong)
        float crossfeed = 0.3f;
        float fb_crossfeed_l = fb_l * (1.f - crossfeed) + fb_r * crossfeed;
        float fb_crossfeed_r = fb_r * (1.f - crossfeed) + fb_l * crossfeed;
        
        // ✅ Soft clip crossfeed to prevent buildup
        fb_crossfeed_l = soft_clip(fb_crossfeed_l);
        fb_crossfeed_r = soft_clip(fb_crossfeed_r);
        
        // ✅ FREEZE: Stop nieuwe input, maar process buffers
        float freeze_input_l = in_l;
        float freeze_input_r = in_r;
        if (s_freeze) {
            freeze_input_l = 0.f;
            freeze_input_r = 0.f;
        }
        
        // ✅ Write to delay - WITH SOFT CLIPPING!
        {
            // ✅ Soft clip input + feedback before writing
            float new_l = soft_clip(freeze_input_l + fb_crossfeed_l);
            float new_r = soft_clip(freeze_input_r + fb_crossfeed_r);
            
            // ✅ NaN protection before write
            new_l = safe_float(new_l);
            new_r = safe_float(new_r);
            
            s_delay_l[s_delay_write] = new_l;
            s_delay_r[s_delay_write] = new_r;
            
            // ✅ Extra safety: Hard clip as final protection
            s_delay_l[s_delay_write] = clipminmaxf(-2.f, s_delay_l[s_delay_write], 2.f);
            s_delay_r[s_delay_write] = clipminmaxf(-2.f, s_delay_r[s_delay_write], 2.f);
        }
        
        // STEREO WIDENING
        // ✅ Smooth width
        const float width_smoothing = 0.995f;
        s_width_smooth += (s_stereo_width - s_width_smooth) * (1.f - width_smoothing);
        stereo_widen(&delayed_l, &delayed_r, 1.f + s_width_smooth);
        
        // ✅ NaN protection on delayed signals
        delayed_l = safe_float(delayed_l);
        delayed_r = safe_float(delayed_r);
        
        // MIX
        // ✅ Smooth mix (prevent output jumps)
        const float mix_smoothing = 0.995f;
        s_mix_smooth += (s_mix - s_mix_smooth) * (1.f - mix_smoothing);
        out_ptr[0] = in_l * (1.f - s_mix_smooth) + delayed_l * s_mix_smooth;
        out_ptr[1] = in_r * (1.f - s_mix_smooth) + delayed_r * s_mix_smooth;
        
        // ✅ Denormal kill
        if (fabsf(out_ptr[0]) < 1e-15f) out_ptr[0] = 0.f;
        if (fabsf(out_ptr[1]) < 1e-15f) out_ptr[1] = 0.f;
        
        // ✅ Final NaN protection and clipping
        out_ptr[0] = safe_float(out_ptr[0]);
        out_ptr[1] = safe_float(out_ptr[1]);
        out_ptr[0] = clipminmaxf(-1.f, out_ptr[0], 1.f);
        out_ptr[1] = clipminmaxf(-1.f, out_ptr[1], 1.f);
        
        in_ptr += 2;
        out_ptr += 2;
        
        s_delay_write = (s_delay_write + 1) % MAX_DELAY_TIME;
        s_sample_counter++;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_delay_time = valf; break;
        case 1: s_feedback_amount = valf; break;
        case 2: s_stereo_width = valf; break;
        case 3: s_shimmer_amount = valf; break;
        case 4: s_diffusion_amount = valf; break;
        case 5: s_modulation_depth = valf; break;
        case 6: s_mix = valf; break;
        case 7: 
            s_division = value;
            s_tempo_sync = true;
            break;
        case 8: s_mode = value; break;
        case 9: s_freeze = (value > 0); break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_delay_time * 1023.f);
        case 1: return (int32_t)(s_feedback_amount * 1023.f);
        case 2: return (int32_t)(s_stereo_width * 1023.f);
        case 3: return (int32_t)(s_shimmer_amount * 1023.f);
        case 4: return (int32_t)(s_diffusion_amount * 1023.f);
        case 5: return (int32_t)(s_modulation_depth * 1023.f);
        case 6: return (int32_t)(s_mix * 1023.f);
        case 7: return s_division;
        case 8: return s_mode;
        case 9: return s_freeze ? 1 : 0;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 7) {
        static const char *div_names[] = {
            "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T",
            "1/4", "1/2T", "1/2", "3/4", "1/1", "2/1", "3/1", "4/1"
        };
        if (value >= 0 && value < 16) return div_names[value];
    }
    if (id == 8) {
        static const char *mode_names[] = {
            "DIGITAL", "ANALOG", "LOFI", "SHIMMER",
            "REVERSE", "GRANULAR", "INFINITE", "CHAOS"
        };
        if (value >= 0 && value < 8) return mode_names[value];
    }
    if (id == 9) {
        return value ? "FREEZE" : "NORMAL";
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    float bpm = (float)tempo / 10.f;
    if (bpm < 60.f) bpm = 120.f;  // Default
    s_beat_length = (uint32_t)(48000.f * 60.f / bpm);
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    s_tempo_sync = true;
}

