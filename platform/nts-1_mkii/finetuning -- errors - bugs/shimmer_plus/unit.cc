/*
    SHIMMER+ REVERB - Advanced pitch-shifting reverb
    
    ALGORITHM:
    1. Input → Pre-delay → HPF/LPF
    2. Comb filters (4x parallel) with damping
    3. Pitch shifter (+12 semitones)
    4. Allpass diffusion (8x cascade)
    5. Modulation (LFO on delay times)
    6. Envelope follower → Ducking
    7. Freeze mode (feedback = 0.93)
    8. Output limiting
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_revfx.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "utils/buffer_ops.h"
#include "fx_api.h"
#include "macros.h"
#include <algorithm>

// SDK compatibility - PI is already defined in CMSIS arm_math.h
#ifndef PI
#define PI 3.14159265359f
#endif

// ✅ Soft clipper for feedback loops
inline float soft_clip(float x) {
    if (x < -1.5f) return -1.f;
    if (x > 1.5f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

#define NUM_COMBS 4
#define NUM_ALLPASS 8
#define PREDELAY_SIZE 4800   // 100ms @ 48kHz (reduced from 500ms)
#define PITCH_BUFFER_SIZE 1024  // Reduced from 4096
#define LFO_TABLE_SIZE 128   // Reduced from 256
#define MAX_COMB_SIZE 1200   // Max comb buffer size (~25ms delay)
#define MAX_ALLPASS_SIZE 512 // Max allpass buffer size

// Comb filter delays (prime numbers for density) - reduced for memory limits
static const uint32_t s_comb_delays[NUM_COMBS] = {
    557, 617, 491, 422  // Reduced from 1557, 1617, 1491, 1422 (~12ms delays)
};

// Allpass delays (prime numbers)
static const uint32_t s_allpass_delays[NUM_ALLPASS] = {
    225, 341, 441, 556, 673, 787, 911, 1031
};

struct CombFilter {
    uint32_t write_pos;
    uint32_t delay_length;
    float feedback;
    float damp_z;
    float *buffer;
};

struct AllpassFilter {
    uint32_t write_pos;
    uint32_t delay_length;
    float feedback;
    float *buffer;
    float z1;  // ✅ DC blocker state
};

struct PitchShifter {
    float buffer[PITCH_BUFFER_SIZE];
    uint32_t write_pos;
    float read_pos;
    float pitch_ratio;
    float lpf_z1;  // ✅ Anti-aliasing filter state
};

static CombFilter s_combs_l[NUM_COMBS];
static CombFilter s_combs_r[NUM_COMBS];
static AllpassFilter s_allpass_l[NUM_ALLPASS];
static AllpassFilter s_allpass_r[NUM_ALLPASS];
static PitchShifter s_pitch_l;
static PitchShifter s_pitch_r;

static float *s_predelay_buffer;
static uint32_t s_predelay_write;

// LFO for modulation
static float s_lfo_table[LFO_TABLE_SIZE];
static float s_lfo_phase;

// Envelope follower
static float s_envelope;
static float s_envelope_attack;
static float s_envelope_release;

// Filters
static float s_lpf_z1_l, s_lpf_z1_r;
static float s_hpf_z1_l, s_hpf_z1_r;

// Parameters
static float s_time;
static float s_shimmer_amount;
static float s_mix;
static float s_mod_rate;
static float s_mod_depth;
static float s_lp_cutoff;
static float s_hp_cutoff;
static float s_predelay_time;
static float s_duck_amount;
static bool s_freeze;
static uint8_t s_mode;

static uint32_t s_sample_counter;

// Initialize LFO table
void init_lfo_table() {
    for (int i = 0; i < LFO_TABLE_SIZE; i++) {
        float phase = (float)i / (float)LFO_TABLE_SIZE;
        float angle = (phase - 0.5f) * 2.f * PI;
        s_lfo_table[i] = fx_sinf(angle);
    }
}

// Read LFO with interpolation
inline float lfo_read(float phase) {
    phase -= (int32_t)phase;
    if (phase < 0.f) phase += 1.f;
    if (phase >= 1.f) phase -= 1.f;
    
    float idx_f = phase * (float)(LFO_TABLE_SIZE - 1);
    uint32_t idx0 = (uint32_t)idx_f;
    uint32_t idx1 = (idx0 + 1) % LFO_TABLE_SIZE;
    float frac = idx_f - (float)idx0;
    
    return s_lfo_table[idx0] * (1.f - frac) + s_lfo_table[idx1] * frac;
}

// Soft clipper removed - already defined above

// One-pole lowpass
inline float lpf_process(float input, float cutoff, float *z1) {
    float coeff = 1.f - fastexpf(-2.f * PI * cutoff / 48000.f);
    coeff = clipminmaxf(0.001f, coeff, 0.999f);
    *z1 = *z1 + coeff * (input - *z1);
    return *z1;
}

// One-pole highpass
inline float hpf_process(float input, float cutoff, float *z1) {
    float coeff = fastexpf(-2.f * PI * cutoff / 48000.f);
    coeff = clipminmaxf(0.001f, coeff, 0.999f);
    float output = input - *z1;
    *z1 = *z1 + coeff * output;
    return output;
}

// Comb filter with damping
inline float comb_process(CombFilter *c, float input) {
    // ✅ Input limiting
    input = clipminmaxf(-1.f, input, 1.f);
    
    uint32_t read_pos = c->write_pos;
    if (read_pos >= c->delay_length) read_pos = 0;  // ✅ Bounds check
    float delayed = c->buffer[read_pos];
    
    // ✅ Denormal check
    if (fabsf(delayed) < 1e-15f) delayed = 0.f;
    
    // One-pole damping filter
    c->damp_z = c->damp_z * 0.7f + delayed * 0.3f;
    float damped = c->damp_z;
    
    // ✅ Feedback MUST be < 0.93 (was 0.95 - te hoog voor stability!)
    float fb = clipminmaxf(0.f, c->feedback, 0.93f);
    
    // Feedback with soft clipping
    float feedback_signal = damped * fb;
    feedback_signal = soft_clip(feedback_signal);
    feedback_signal = clipminmaxf(-2.f, feedback_signal, 2.f);  // ✅ Extra limit
    
    c->buffer[c->write_pos] = input + feedback_signal;
    c->write_pos = (c->write_pos + 1) % c->delay_length;
    
    // ✅ Output limiting
    float output = delayed;
    output = clipminmaxf(-2.f, output, 2.f);
    
    // ✅ Denormal kill
    if (fabsf(output) < 1e-15f) output = 0.f;
    
    return output;
}

// Allpass filter
inline float allpass_process(AllpassFilter *ap, float input) {
    // ✅ Input limiting
    input = clipminmaxf(-2.f, input, 2.f);
    
    uint32_t read_pos = ap->write_pos;
    if (read_pos >= ap->delay_length) read_pos = 0;  // ✅ Bounds check
    
    float delayed = ap->buffer[read_pos];
    
    // ✅ Denormal check
    if (fabsf(delayed) < 1e-15f) delayed = 0.f;
    
    float output = -input + delayed;
    
    // ✅ KRITIEK: All-pass feedback MUST be < 0.65!
    float ap_fb = clipminmaxf(0.2f, ap->feedback, 0.65f);  // MAX 0.65!
    float fb_signal = delayed * ap_fb;
    fb_signal = clipminmaxf(-2.f, fb_signal, 2.f);  // ✅ Limit feedback signal
    
    ap->buffer[ap->write_pos] = input + fb_signal;
    
    ap->write_pos = (ap->write_pos + 1) % ap->delay_length;
    
    // ✅ DC blocker
    if (!ap->z1) ap->z1 = 0.f;  // Init if needed
    ap->z1 = ap->z1 * 0.995f + output * 0.005f;
    output = output - ap->z1;
    
    // ✅ Output limiting
    output = clipminmaxf(-2.f, output, 2.f);
    
    // ✅ Denormal kill
    if (fabsf(output) < 1e-15f) output = 0.f;
    
    return output;
}

// Pitch shifter (+12 semitones for shimmer)
inline float pitch_shift_process(PitchShifter *ps, float input) {
    // ✅ BYPASS als shimmer amount te laag
    if (s_shimmer_amount < 0.01f) {
        return input;  // Direct passthrough
    }
    
    // Input limiting (prevent spikes)
    input = clipminmaxf(-1.f, input, 1.f);
    
    // Write input
    ps->buffer[ps->write_pos] = input;
    uint32_t old_write = ps->write_pos;
    ps->write_pos = (ps->write_pos + 1) % PITCH_BUFFER_SIZE;
    
    // ✅ KRITIEK: Check minimum distance
    int32_t distance = (int32_t)old_write - (int32_t)ps->read_pos;
    if (distance < 0) distance += PITCH_BUFFER_SIZE;
    
    if (distance < 1000) {  // ✅ 1000 samples minimum! (was 256)
        // TOO CLOSE - return silence to prevent noise
        return 0.f;
    }
    
    // Read with safety checks
    uint32_t read_pos_0 = (uint32_t)ps->read_pos;
    if (read_pos_0 >= PITCH_BUFFER_SIZE) read_pos_0 = 0;  // ✅ Bounds check
    
    uint32_t read_pos_1 = (read_pos_0 + 1) % PITCH_BUFFER_SIZE;
    float frac = ps->read_pos - (float)read_pos_0;
    frac = clipminmaxf(0.f, frac, 1.f);  // ✅ Clamp interpolation
    
    float output = ps->buffer[read_pos_0] * (1.f - frac) + 
                   ps->buffer[read_pos_1] * frac;
    
    // ✅ Output limiting
    output = clipminmaxf(-2.f, output, 2.f);
    
    // Advance read position
    ps->read_pos += ps->pitch_ratio;
    
    // ✅ Wrap with bounds check
    while (ps->read_pos >= (float)PITCH_BUFFER_SIZE) {
        ps->read_pos -= (float)PITCH_BUFFER_SIZE;
    }
    while (ps->read_pos < 0.f) {
        ps->read_pos += (float)PITCH_BUFFER_SIZE;
    }
    
    // ✅ Anti-aliasing low-pass
    float lpf_coeff = 0.2f;  // Strong filtering
    ps->lpf_z1 += (output - ps->lpf_z1) * lpf_coeff;
    output = ps->lpf_z1;
    
    // ✅ Soft clip
    if (output > 1.f) output = 1.f;
    if (output < -1.f) output = -1.f;
    
    return output * 0.5f;  // ✅ Extra attenuation
}

// Envelope follower
inline float envelope_follow(float input) {
    float rectified = si_fabsf(input);
    
    if (rectified > s_envelope) {
        // Attack
        s_envelope += (rectified - s_envelope) * s_envelope_attack;
    } else {
        // Release
        s_envelope += (rectified - s_envelope) * s_envelope_release;
    }
    
    return s_envelope;
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;

    if (!desc->hooks.sdram_alloc) return k_unit_err_memory;
    
    // Calculate memory requirements (reduced for NTS-1 mkII memory limits)
    uint32_t total_samples = NUM_COMBS * 2 * MAX_COMB_SIZE + 
                             NUM_ALLPASS * 2 * MAX_ALLPASS_SIZE + 
                             PREDELAY_SIZE;
    
    float *reverb_buf = (float *)desc->hooks.sdram_alloc(total_samples * sizeof(float));
    if (!reverb_buf) return k_unit_err_memory;
    
    // ✅ KRITIEK: WIS ALLE BUFFERS!
    std::fill(reverb_buf, reverb_buf + total_samples, 0.f);
    
    // Assign buffers
    float *buf_ptr = reverb_buf;
    
    for (int i = 0; i < NUM_COMBS; i++) {
        s_combs_l[i].buffer = buf_ptr;
        // ✅ WIS COMB BUFFER
        std::fill(buf_ptr, buf_ptr + MAX_COMB_SIZE, 0.f);
        buf_ptr += MAX_COMB_SIZE;
        
        s_combs_r[i].buffer = buf_ptr;
        // ✅ WIS COMB BUFFER
        std::fill(buf_ptr, buf_ptr + MAX_COMB_SIZE, 0.f);
        buf_ptr += MAX_COMB_SIZE;
        
        s_combs_l[i].write_pos = 0;
        s_combs_l[i].delay_length = s_comb_delays[i];
        s_combs_l[i].feedback = 0.84f;
        s_combs_l[i].damp_z = 0.f;
        
        s_combs_r[i].write_pos = 0;
        s_combs_r[i].delay_length = s_comb_delays[i] + 17;  // Reduced stereo offset
        s_combs_r[i].feedback = 0.84f;
        s_combs_r[i].damp_z = 0.f;
    }
    
    for (int i = 0; i < NUM_ALLPASS; i++) {
        s_allpass_l[i].buffer = buf_ptr;
        // ✅ WIS ALLPASS BUFFER
        std::fill(buf_ptr, buf_ptr + MAX_ALLPASS_SIZE, 0.f);
        buf_ptr += MAX_ALLPASS_SIZE;
        
        s_allpass_r[i].buffer = buf_ptr;
        // ✅ WIS ALLPASS BUFFER
        std::fill(buf_ptr, buf_ptr + MAX_ALLPASS_SIZE, 0.f);
        buf_ptr += MAX_ALLPASS_SIZE;
        
        s_allpass_l[i].write_pos = 0;
        s_allpass_l[i].delay_length = s_allpass_delays[i];
        s_allpass_l[i].feedback = 0.5f;  // ✅ Init 0.5 (max 0.65)
        s_allpass_l[i].z1 = 0.f;  // ✅ Init DC blocker
        
        s_allpass_r[i].write_pos = 0;
        s_allpass_r[i].delay_length = s_allpass_delays[i] + 17;
        s_allpass_r[i].feedback = 0.5f;  // ✅ Init 0.5 (max 0.65)
        s_allpass_r[i].z1 = 0.f;  // ✅ Init DC blocker
    }
    
    s_predelay_buffer = buf_ptr;
    // ✅ WIS PREDELAY BUFFER
    std::fill(buf_ptr, buf_ptr + PREDELAY_SIZE, 0.f);
    s_predelay_write = 0;
    
    // ✅ FORCE pitch shifter OFF at init
    s_shimmer_amount = 0.f;  // NIET 0.2f!
    
    // Init pitch shifters met SAFE waarden
    std::fill(s_pitch_l.buffer, s_pitch_l.buffer + PITCH_BUFFER_SIZE, 0.f);
    std::fill(s_pitch_r.buffer, s_pitch_r.buffer + PITCH_BUFFER_SIZE, 0.f);
    s_pitch_l.write_pos = 0;
    s_pitch_l.read_pos = 100.f;  // ✅ START 100 samples ahead!
    s_pitch_l.pitch_ratio = 1.f;
    s_pitch_l.lpf_z1 = 0.f;
    
    s_pitch_r.write_pos = 0;
    s_pitch_r.read_pos = 100.f;  // ✅ START 100 samples ahead!
    s_pitch_r.pitch_ratio = 1.f;
    s_pitch_r.lpf_z1 = 0.f;
    
    init_lfo_table();
    s_lfo_phase = 0.f;
    
    s_envelope = 0.f;
    s_envelope_attack = 0.01f;
    s_envelope_release = 0.001f;
    
    s_lpf_z1_l = s_lpf_z1_r = 0.f;
    s_hpf_z1_l = s_hpf_z1_r = 0.f;
    
    // ✅ FIX: Init parameters with safe defaults
    s_time = 0.6f;
    s_shimmer_amount = 0.f;  // ✅ 0% default - FORCE OFF!
    s_mix = 0.6f;  // 60% wet default
    s_mod_rate = 0.4f;
    s_mod_depth = 0.3f;
    s_lp_cutoff = 0.5f;
    s_hp_cutoff = 0.25f;
    s_predelay_time = 0.35f;
    s_duck_amount = 0.4f;
    s_freeze = false;
    s_mode = 0;
    
    s_sample_counter = 0;
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    // ✅ Reset ALL filter/delay states
    for (int i = 0; i < NUM_COMBS; i++) {
        s_combs_l[i].write_pos = 0;
        s_combs_l[i].damp_z = 0.f;
        s_combs_r[i].write_pos = 0;
        s_combs_r[i].damp_z = 0.f;
        
        // Clear comb buffers
        if (s_combs_l[i].buffer) {
            std::fill(s_combs_l[i].buffer, 
                     s_combs_l[i].buffer + s_combs_l[i].delay_length, 0.f);
        }
        if (s_combs_r[i].buffer) {
            std::fill(s_combs_r[i].buffer, 
                     s_combs_r[i].buffer + s_combs_r[i].delay_length, 0.f);
        }
    }
    
    for (int i = 0; i < NUM_ALLPASS; i++) {
        s_allpass_l[i].write_pos = 0;
        s_allpass_l[i].z1 = 0.f;  // ✅ Reset DC blocker
        s_allpass_r[i].write_pos = 0;
        s_allpass_r[i].z1 = 0.f;  // ✅ Reset DC blocker
        
        // Clear allpass buffers
        if (s_allpass_l[i].buffer) {
            std::fill(s_allpass_l[i].buffer, 
                     s_allpass_l[i].buffer + s_allpass_l[i].delay_length, 0.f);
        }
        if (s_allpass_r[i].buffer) {
            std::fill(s_allpass_r[i].buffer, 
                     s_allpass_r[i].buffer + s_allpass_r[i].delay_length, 0.f);
        }
    }
    
    // Clear predelay
    if (s_predelay_buffer) {
        std::fill(s_predelay_buffer, s_predelay_buffer + PREDELAY_SIZE, 0.f);
    }
    s_predelay_write = 0;
    
    // Reset pitch shifters
    s_pitch_l.write_pos = 0;
    s_pitch_l.read_pos = 100.f;  // ✅ Safe distance
    s_pitch_l.lpf_z1 = 0.f;
    
    s_pitch_r.write_pos = 0;
    s_pitch_r.read_pos = 100.f;  // ✅ Safe distance
    s_pitch_r.lpf_z1 = 0.f;
    
    // Clear pitch buffers
    std::fill(s_pitch_l.buffer, s_pitch_l.buffer + PITCH_BUFFER_SIZE, 0.f);
    std::fill(s_pitch_r.buffer, s_pitch_r.buffer + PITCH_BUFFER_SIZE, 0.f);
    
    s_envelope = 0.f;
    s_lpf_z1_l = s_lpf_z1_r = 0.f;
    s_hpf_z1_l = s_hpf_z1_r = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    for (uint32_t f = 0; f < frames; f++) {
        float in_l = clipminmaxf(-1.f, in[f * 2], 1.f);
        float in_r = clipminmaxf(-1.f, in[f * 2 + 1], 1.f);
        
        // ✅ KRITIEK: Detect COMPLETE silence
        float input_level = fabsf(in_l) + fabsf(in_r);
        
        // Als freeze NIET actief EN input stil → skip processing
        if (!s_freeze && input_level < 0.0001f) {
            // ✅ Decay buffers slowly (prevent stuck ruis)
            // Maar output = silence
            out[f * 2] = 0.f;
            out[f * 2 + 1] = 0.f;
            continue;  // Skip processing
        }
        
        // Envelope follower
        float env = envelope_follow((in_l + in_r) * 0.5f);
        
        // Pre-delay
        uint32_t predelay_samples = (uint32_t)(s_predelay_time * (float)PREDELAY_SIZE);
        if (predelay_samples >= PREDELAY_SIZE) predelay_samples = PREDELAY_SIZE - 1;
        uint32_t predelay_read = (s_predelay_write + PREDELAY_SIZE - predelay_samples) % PREDELAY_SIZE;
        
        float predelayed = s_predelay_buffer[predelay_read];
        s_predelay_buffer[s_predelay_write] = (in_l + in_r) * 0.5f;
        s_predelay_write = (s_predelay_write + 1) % PREDELAY_SIZE;
        
        // Filters
        float lp_freq = 1000.f + s_lp_cutoff * 19000.f;
        float hp_freq = 20.f + s_hp_cutoff * 980.f;
        
        float filtered_l = hpf_process(predelayed, hp_freq, &s_hpf_z1_l);
        filtered_l = lpf_process(filtered_l, lp_freq, &s_lpf_z1_l);
        
        float filtered_r = hpf_process(predelayed, hp_freq, &s_hpf_z1_r);
        filtered_r = lpf_process(filtered_r, lp_freq, &s_lpf_z1_r);
        
        // ✅ ABSOLUTE MAXIMUM 0.93 (was 0.95 - te hoog voor stability!)
        float fb = s_freeze ? 0.93f : (0.65f + s_time * 0.25f);  // ✅ FREEZE = 0.93, NIET 0.95!
        fb = clipminmaxf(0.1f, fb, 0.93f);  // ✅ LOWER LIMIT!
        
        for (int i = 0; i < NUM_COMBS; i++) {
            s_combs_l[i].feedback = fb;
            s_combs_r[i].feedback = fb;
        }
        
        // Modulation (LFO on comb delay times)
        s_lfo_phase += s_mod_rate * 0.001f;
        if (s_lfo_phase >= 1.f) s_lfo_phase -= 1.f;
        
        float lfo_val = lfo_read(s_lfo_phase) * s_mod_depth * 0.1f;
        
        float comb_out_l = 0.f;
        float comb_out_r = 0.f;
        
        for (int i = 0; i < NUM_COMBS; i++) {
            // Apply modulation to delay length
            float mod_scale = 1.f + lfo_val * (float)(i % 2 == 0 ? 1 : -1);
            uint32_t delay_l = (uint32_t)((float)s_comb_delays[i] * mod_scale);
            uint32_t delay_r = (uint32_t)((float)(s_comb_delays[i] + 17) * mod_scale);
            // Clamp to prevent buffer overflow
            s_combs_l[i].delay_length = clipminmaxi32(1, delay_l, MAX_COMB_SIZE - 1);
            s_combs_r[i].delay_length = clipminmaxi32(1, delay_r, MAX_COMB_SIZE - 1);
            
            comb_out_l += comb_process(&s_combs_l[i], filtered_l);
            comb_out_r += comb_process(&s_combs_r[i], filtered_r);
        }
        
        comb_out_l /= (float)NUM_COMBS;
        comb_out_r /= (float)NUM_COMBS;
        
        // ✅ FIX: Update pitch ratio with safe limiting (max 1.5×, not 2.0×!)
        float pitch_ratio = 1.f + s_shimmer_amount * 0.5f;  // 0% = 1.0×, 100% = 1.5×
        pitch_ratio = clipminmaxf(1.f, pitch_ratio, 1.5f);  // Hard limit to 1.5×
        s_pitch_l.pitch_ratio = pitch_ratio;
        s_pitch_r.pitch_ratio = pitch_ratio;
        
        // Pitch shifter (shimmer effect)
        float shimmer_l = pitch_shift_process(&s_pitch_l, comb_out_l);
        float shimmer_r = pitch_shift_process(&s_pitch_r, comb_out_r);
        
        // ✅ FIX: Shimmer mix with gain reduction (max 30%, not 100%!)
        float shimmer_mix = s_shimmer_amount * 0.3f;  // Max 30% shimmer
        shimmer_mix = clipminmaxf(0.f, shimmer_mix, 0.3f);
        
        // ✅ Soft clip shimmer signals before mixing
        shimmer_l = soft_clip(shimmer_l);
        shimmer_r = soft_clip(shimmer_r);
        
        comb_out_l = comb_out_l * (1.f - shimmer_mix) + shimmer_l * shimmer_mix;
        comb_out_r = comb_out_r * (1.f - shimmer_mix) + shimmer_r * shimmer_mix;
        
        // ✅ Extra safety: Soft clip combined output
        comb_out_l = soft_clip(comb_out_l);
        comb_out_r = soft_clip(comb_out_r);
        
        // Allpass diffusion
        for (int i = 0; i < NUM_ALLPASS; i++) {
            comb_out_l = allpass_process(&s_allpass_l[i], comb_out_l);
            comb_out_r = allpass_process(&s_allpass_r[i], comb_out_r);
        }
        
        // Soft clip
        float wet_l = soft_clip(comb_out_l * 0.5f);
        float wet_r = soft_clip(comb_out_r * 0.5f);
        
        // ✅ NaN/Inf detection (using manual check - no std::isfinite in SDK)
        // Check for NaN/Inf manually
        if (wet_l != wet_l || wet_l > 1e10f || wet_l < -1e10f) wet_l = 0.f;  // NaN or Inf
        if (wet_r != wet_r || wet_r > 1e10f || wet_r < -1e10f) wet_r = 0.f;  // NaN or Inf
        
        // ✅ Denormal kill
        if (fabsf(wet_l) < 1e-15f) wet_l = 0.f;
        if (fabsf(wet_r) < 1e-15f) wet_r = 0.f;
        
        // ✅ Hard limiting
        wet_l = clipminmaxf(-1.f, wet_l, 1.f);
        wet_r = clipminmaxf(-1.f, wet_r, 1.f);
        
        // Ducking (envelope follower reduces wet signal)
        float duck_factor = 1.f - env * s_duck_amount;
        wet_l *= duck_factor;
        wet_r *= duck_factor;
        
        // Mix (constant power - simplified version)
        float dry_wet = (s_mix + 1.f) * 0.5f;
        dry_wet = clipminmaxf(0.f, dry_wet, 1.f);  // ✅ Clamp mix
        
        // Constant power mixing: use cosine/sine approximation for smooth transition
        float dry_gain = 1.f - dry_wet * 0.707f;  // Approximate sqrt(1-dry_wet)
        float wet_gain = dry_wet * 0.707f;        // Approximate sqrt(dry_wet)
        
        // Normalize to prevent volume drop
        float norm = 1.f / (dry_gain + wet_gain + 0.001f);
        dry_gain *= norm;
        wet_gain *= norm;
        
        out[f * 2] = in_l * dry_gain + wet_l * wet_gain;
        out[f * 2 + 1] = in_r * dry_gain + wet_r * wet_gain;
        
        // ✅ FINAL hard limit
        out[f * 2] = clipminmaxf(-1.f, out[f * 2], 1.f);
        out[f * 2 + 1] = clipminmaxf(-1.f, out[f * 2 + 1], 1.f);
        
        // ✅ Emergency shutdown check
        if (fabsf(out[f * 2]) > 2.f || fabsf(out[f * 2 + 1]) > 2.f) {
            // EMERGENCY SHUTDOWN
            out[f * 2] = 0.f;
            out[f * 2 + 1] = 0.f;
        }
        
        s_sample_counter++;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_time = valf; break;
        case 1: s_shimmer_amount = valf; break;
        case 2: s_mix = (float)value / 100.f; break;
        case 3: s_mod_rate = valf; break;
        case 4: s_mod_depth = valf; break;
        case 5: s_lp_cutoff = valf; break;
        case 6: s_hp_cutoff = valf; break;
        case 7: s_predelay_time = valf; break;
        case 8: s_duck_amount = valf; break;
        case 9: s_freeze = (value != 0); break;
        case 10: s_mode = value; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_time * 1023.f);
        case 1: return (int32_t)(s_shimmer_amount * 1023.f);
        case 2: return (int32_t)(s_mix * 100.f);
        case 3: return (int32_t)(s_mod_rate * 1023.f);
        case 4: return (int32_t)(s_mod_depth * 1023.f);
        case 5: return (int32_t)(s_lp_cutoff * 1023.f);
        case 6: return (int32_t)(s_hp_cutoff * 1023.f);
        case 7: return (int32_t)(s_predelay_time * 1023.f);
        case 8: return (int32_t)(s_duck_amount * 1023.f);
        case 9: return s_freeze ? 1 : 0;
        case 10: return s_mode;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 9) {
        return value ? "ON" : "OFF";
    }
    if (id == 10) {
        static const char *mode_names[] = {"SHIMMER", "REVERSE", "CLOUD", "INFINITE"};
        if (value >= 0 && value < 4) return mode_names[value];
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {}
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {}

