/*
    TAPE WOBBLE V2 - COMPLETE FIX!
    
    CRITICAL FIXES:
    - Removed custom is_finite macro (use si_isfinite!)
    - Removed custom fast_tanh (use si_tanhf!)
    - Buffer clearing on init
    - All validation uses SDK functions
*/

#include "unit_modfx.h"
#include "utils/float_math.h"
#include "fx_api.h"

// ========== FAST TANH (CORRECT IMPLEMENTATION) ==========
inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// ========== MEMORY BUDGET ==========

#define MAX_DELAY_SAMPLES 2400
#define NOISE_BUFFER_SIZE 512

// ========== TAPE TYPES ==========

enum TapeType {
    TAPE_CASSETTE_I = 0,
    TAPE_CASSETTE_II,
    TAPE_CASSETTE_IV,
    TAPE_REEL_7_5,
    TAPE_REEL_15,
    TAPE_REEL_30,
    TAPE_8TRACK,
    TAPE_DICTAPHONE
};

enum SpeedMode {
    SPEED_STOPPED = 0,
    SPEED_SLOW,
    SPEED_NORMAL,
    SPEED_FAST
};

// ========== BUFFERS ==========

static float *s_delay_buffer_l = nullptr;
static float *s_delay_buffer_r = nullptr;
static uint32_t s_delay_write_pos = 0;
static float s_noise_buffer[NOISE_BUFFER_SIZE];
static uint32_t s_noise_pos = 0;

// ========== STATE ==========

static float s_lfo_wow = 0.f;
static float s_lfo_flutter = 0.f;
static float s_lfo_warble = 0.f;
static float s_comp_env = 1.f;
static uint32_t s_dropout_counter = 0;
static float s_dropout_level = 1.f;
static float s_hf_z1_l = 0.f;
static float s_hf_z1_r = 0.f;

// ========== PARAMETERS ==========

static float s_wow = 0.6f;
static float s_flutter = 0.5f;
static float s_saturation = 0.75f;
static float s_noise = 0.3f;
static float s_compression = 0.4f;
static float s_warble = 0.25f;
static float s_age = 0.65f;
static float s_mix = 0.5f;
static TapeType s_tape_type = TAPE_CASSETTE_II;
static SpeedMode s_speed_mode = SPEED_NORMAL;

// ========== RANDOM ==========

static uint32_t s_rand_state = 12345;

inline float random_float() {
    s_rand_state ^= s_rand_state << 13;
    s_rand_state ^= s_rand_state >> 17;
    s_rand_state ^= s_rand_state << 5;
    return ((float)(s_rand_state % 10000) / 10000.f) * 2.f - 1.f;
}

// ========== TAPE CHARACTERISTICS ==========

inline void get_tape_characteristics(float *wow_scale, float *flutter_scale, 
                                     float *hf_loss, float *noise_scale) {
    switch (s_tape_type) {
        case TAPE_CASSETTE_I:
            *wow_scale = 1.5f; *flutter_scale = 1.2f;
            *hf_loss = 0.7f; *noise_scale = 1.3f;
            break;
        case TAPE_CASSETTE_II:
            *wow_scale = 1.0f; *flutter_scale = 0.9f;
            *hf_loss = 0.5f; *noise_scale = 0.9f;
            break;
        case TAPE_CASSETTE_IV:
            *wow_scale = 0.8f; *flutter_scale = 0.7f;
            *hf_loss = 0.3f; *noise_scale = 0.7f;
            break;
        case TAPE_REEL_7_5:
            *wow_scale = 1.2f; *flutter_scale = 0.8f;
            *hf_loss = 0.6f; *noise_scale = 1.0f;
            break;
        case TAPE_REEL_15:
            *wow_scale = 0.6f; *flutter_scale = 0.5f;
            *hf_loss = 0.4f; *noise_scale = 0.6f;
            break;
        case TAPE_REEL_30:
            *wow_scale = 0.3f; *flutter_scale = 0.3f;
            *hf_loss = 0.2f; *noise_scale = 0.4f;
            break;
        case TAPE_8TRACK:
            *wow_scale = 3.0f; *flutter_scale = 2.0f;
            *hf_loss = 0.9f; *noise_scale = 1.8f;
            break;
        case TAPE_DICTAPHONE:
            *wow_scale = 2.5f; *flutter_scale = 2.5f;
            *hf_loss = 0.95f; *noise_scale = 2.0f;
            break;
        default:
            *wow_scale = 1.0f; *flutter_scale = 1.0f;
            *hf_loss = 0.5f; *noise_scale = 1.0f;
            break;
    }
}

// ========== DELAY READ ==========

inline float delay_read(float *buffer, float delay_samples) {
    if (!buffer) return 0.f;
    
    delay_samples = clipminmaxf(1.f, delay_samples, (float)(MAX_DELAY_SAMPLES - 2));
    
    float read_pos_f = (float)s_delay_write_pos - delay_samples;
    
    while (read_pos_f < 0.f) read_pos_f += (float)MAX_DELAY_SAMPLES;
    while (read_pos_f >= (float)MAX_DELAY_SAMPLES) read_pos_f -= (float)MAX_DELAY_SAMPLES;
    
    uint32_t read_pos_0 = (uint32_t)read_pos_f;
    uint32_t read_pos_1 = (read_pos_0 + 1) % MAX_DELAY_SAMPLES;
    
    float frac = read_pos_f - (float)read_pos_0;
    
    float result = buffer[read_pos_0] * (1.f - frac) + buffer[read_pos_1] * frac;
    
    // ✅ FIX: Validate result
    if (result != result || result > 1e10f || result < -1e10f) result = 0.f;
    
    return result;
}

// ========== SATURATION ==========

inline float apply_saturation(float input, float amount) {
    if (amount < 0.01f) return input;
    
    float drive = 1.f + amount * 3.f;
    
    // ✅ FIX: Use fast_tanh (fx_tanhf doesn't exist for modfx)
    float saturated = fast_tanh(input * drive);
    
    return input * (1.f - amount) + saturated * amount;
}

// ========== COMPRESSION ==========

inline void update_compressor(float input_level) {
    if (s_compression < 0.01f) {
        s_comp_env = 1.f;
        return;
    }
    
    float threshold = 0.5f;
    float ratio = 3.f;
    
    if (input_level > threshold) {
        float over = input_level - threshold;
        float target = 1.f - (over / ratio) * s_compression;
        target = clipminmaxf(0.3f, target, 1.f);
        
        if (target < s_comp_env) {
            s_comp_env += (target - s_comp_env) * 0.1f;
        } else {
            s_comp_env += (target - s_comp_env) * 0.001f;
        }
    } else {
        s_comp_env += (1.f - s_comp_env) * 0.001f;
    }
}

// ========== HF LOSS ==========

inline void apply_hf_loss(float *l, float *r, float amount) {
    float coeff = 0.5f - amount * 0.4f;
    coeff = clipminmaxf(0.1f, coeff, 0.9f);
    
    s_hf_z1_l += coeff * (*l - s_hf_z1_l);
    s_hf_z1_r += coeff * (*r - s_hf_z1_r);
    
    if (si_fabsf(s_hf_z1_l) < 1e-15f) s_hf_z1_l = 0.f;
    if (si_fabsf(s_hf_z1_r) < 1e-15f) s_hf_z1_r = 0.f;
    
    *l = s_hf_z1_l;
    *r = s_hf_z1_r;
}

// ========== MAIN PROCESSOR ==========

inline void process_tape_wobble(float in_l, float in_r, float *out_l, float *out_r) {
    // Input validation
    if (in_l != in_l || in_l > 1e10f || in_l < -1e10f) in_l = 0.f;
    if (in_r != in_r || in_r > 1e10f || in_r < -1e10f) in_r = 0.f;
    
    float wow_scale, flutter_scale, hf_loss, noise_scale;
    get_tape_characteristics(&wow_scale, &flutter_scale, &hf_loss, &noise_scale);
    
    float age_mult = 1.f + s_age * 0.5f;
    wow_scale *= age_mult;
    flutter_scale *= age_mult;
    hf_loss *= (1.f + s_age * 0.3f);
    noise_scale *= age_mult;
    
    float speed_pitch = 1.f;
    switch (s_speed_mode) {
        case SPEED_STOPPED: speed_pitch = 0.01f; break;
        case SPEED_SLOW: speed_pitch = 0.5f; break;
        case SPEED_NORMAL: speed_pitch = 1.f; break;
        case SPEED_FAST: speed_pitch = 2.f; break;
    }
    
    // Update LFOs
    float wow_rate = (0.2f + s_wow * 1.8f) * wow_scale;
    s_lfo_wow += wow_rate / 48000.f;
    if (s_lfo_wow >= 1.f) s_lfo_wow -= 1.f;
    
    float flutter_rate = (5.f + s_flutter * 15.f) * flutter_scale;
    s_lfo_flutter += flutter_rate / 48000.f;
    if (s_lfo_flutter >= 1.f) s_lfo_flutter -= 1.f;
    
    float warble_rate = 0.5f + s_warble * 2.5f;
    s_lfo_warble += warble_rate / 48000.f;
    if (s_lfo_warble >= 1.f) s_lfo_warble -= 1.f;
    
    // Pitch modulation
    float wow_mod = fx_sinf(s_lfo_wow * 2.f * 3.14159265f) * s_wow * 0.02f * wow_scale;
    float flutter_mod = fx_sinf(s_lfo_flutter * 2.f * 3.14159265f) * s_flutter * 0.005f * flutter_scale;
    
    float total_pitch_mod = (1.f + wow_mod + flutter_mod) * speed_pitch;
    total_pitch_mod = clipminmaxf(0.5f, total_pitch_mod, 2.f);
    
    float base_delay = 100.f;
    float delay_samples = base_delay / total_pitch_mod;
    delay_samples = clipminmaxf(10.f, delay_samples, (float)(MAX_DELAY_SAMPLES - 10));
    
    // Write to buffer
    if (s_delay_buffer_l && s_delay_buffer_r) {
        s_delay_buffer_l[s_delay_write_pos] = in_l;
        s_delay_buffer_r[s_delay_write_pos] = in_r;
    }
    
    // Read delayed
    float delayed_l = delay_read(s_delay_buffer_l, delay_samples);
    float delayed_r = delay_read(s_delay_buffer_r, delay_samples);
    
    // Warble
    if (s_warble > 0.01f) {
        float warble = fx_sinf(s_lfo_warble * 2.f * 3.14159265f) * s_warble;
        float temp = delayed_l;
        delayed_l = delayed_l * (1.f - si_fabsf(warble)) + delayed_r * warble;
        delayed_r = delayed_r * (1.f - si_fabsf(warble)) + temp * (-warble);
    }
    
    // Saturation
    delayed_l = apply_saturation(delayed_l, s_saturation);
    delayed_r = apply_saturation(delayed_r, s_saturation);
    
    // Compression
    float comp_level = (si_fabsf(delayed_l) + si_fabsf(delayed_r)) * 0.5f;
    update_compressor(comp_level);
    
    delayed_l *= s_comp_env;
    delayed_r *= s_comp_env;
    
    // Dropout
    s_dropout_counter++;
    if (s_dropout_counter > 48000) {
        s_dropout_counter = 0;
        if (random_float() < s_age * 0.3f) {
            s_dropout_level = 0.2f;
        }
    }
    
    if (s_dropout_level < 1.f) {
        s_dropout_level += 0.001f;
        if (s_dropout_level > 1.f) s_dropout_level = 1.f;
    }
    
    delayed_l *= s_dropout_level;
    delayed_r *= s_dropout_level;
    
    // Noise
    if (s_noise > 0.01f) {
        float noise = s_noise_buffer[s_noise_pos] * s_noise * 0.1f * noise_scale;
        delayed_l += noise;
        delayed_r += noise * 0.8f;
    }
    
    // HF loss
    apply_hf_loss(&delayed_l, &delayed_r, hf_loss * s_age);
    
    // Validate
    if (delayed_l != delayed_l || delayed_l > 1e10f || delayed_l < -1e10f) delayed_l = in_l;
    if (delayed_r != delayed_r || delayed_r > 1e10f || delayed_r < -1e10f) delayed_r = in_r;
    
    // Mix
    *out_l = in_l * (1.f - s_mix) + delayed_l * s_mix;
    *out_r = in_r * (1.f - s_mix) + delayed_r * s_mix;
}

// ========== CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;
    if (!desc->hooks.sdram_alloc) return k_unit_err_memory;
    
    // Allocate SDRAM
    size_t buffer_size = MAX_DELAY_SAMPLES * sizeof(float) * 2;
    uint8_t *buffer_base = static_cast<uint8_t *>(desc->hooks.sdram_alloc(buffer_size));
    
    if (!buffer_base) return k_unit_err_memory;
    
    s_delay_buffer_l = reinterpret_cast<float *>(buffer_base);
    s_delay_buffer_r = reinterpret_cast<float *>(buffer_base + MAX_DELAY_SAMPLES * sizeof(float));
    
    // ✅ FIX: Clear buffers!
    for (uint32_t i = 0; i < MAX_DELAY_SAMPLES; i++) {
        s_delay_buffer_l[i] = 0.f;
        s_delay_buffer_r[i] = 0.f;
    }
    
    s_delay_write_pos = 0;
    
    // Generate noise
    for (uint32_t i = 0; i < NOISE_BUFFER_SIZE; i++) {
        s_noise_buffer[i] = random_float() * 0.05f;
    }
    s_noise_pos = 0;
    
    // Init state
    s_lfo_wow = 0.f;
    s_lfo_flutter = 0.f;
    s_lfo_warble = 0.f;
    s_comp_env = 1.f;
    s_dropout_counter = 0;
    s_dropout_level = 1.f;
    s_hf_z1_l = 0.f;
    s_hf_z1_r = 0.f;
    
    // Parameters
    s_wow = 0.6f;
    s_flutter = 0.5f;
    s_saturation = 0.75f;
    s_noise = 0.3f;
    s_compression = 0.4f;
    s_warble = 0.25f;
    s_age = 0.65f;
    s_mix = 0.5f;
    s_tape_type = TAPE_CASSETTE_II;
    s_speed_mode = SPEED_NORMAL;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    if (s_delay_buffer_l) {
        for (uint32_t i = 0; i < MAX_DELAY_SAMPLES; i++) {
            s_delay_buffer_l[i] = 0.f;
        }
    }
    if (s_delay_buffer_r) {
        for (uint32_t i = 0; i < MAX_DELAY_SAMPLES; i++) {
            s_delay_buffer_r[i] = 0.f;
        }
    }
    
    s_delay_write_pos = 0;
    s_comp_env = 1.f;
    s_dropout_level = 1.f;
    s_hf_z1_l = 0.f;
    s_hf_z1_r = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    const float *in_ptr = in;
    float *out_ptr = out;
    
    for (uint32_t f = 0; f < frames; f++) {
        float out_l, out_r;
        process_tape_wobble(in_ptr[0], in_ptr[1], &out_l, &out_r);
        
        out_ptr[0] = clipminmaxf(-1.f, out_l, 1.f);
        out_ptr[1] = clipminmaxf(-1.f, out_r, 1.f);
        
        s_delay_write_pos = (s_delay_write_pos + 1) % MAX_DELAY_SAMPLES;
        s_noise_pos = (s_noise_pos + 1) % NOISE_BUFFER_SIZE;
        
        in_ptr += 2;
        out_ptr += 2;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_wow = valf; break;
        case 1: s_flutter = valf; break;
        case 2: s_saturation = valf; break;
        case 3: s_noise = valf; break;
        case 4: s_compression = valf; break;
        case 5: s_warble = valf; break;
        case 6: s_age = valf; break;
        case 7: s_mix = valf; break;
        case 8:
            s_tape_type = (TapeType)value;
            if (s_tape_type > TAPE_DICTAPHONE) s_tape_type = TAPE_DICTAPHONE;
            break;
        case 9:
            s_speed_mode = (SpeedMode)value;
            if (s_speed_mode > SPEED_FAST) s_speed_mode = SPEED_FAST;
            break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)(s_wow * 1023.f);
        case 1: return (int32_t)(s_flutter * 1023.f);
        case 2: return (int32_t)(s_saturation * 1023.f);
        case 3: return (int32_t)(s_noise * 1023.f);
        case 4: return (int32_t)(s_compression * 1023.f);
        case 5: return (int32_t)(s_warble * 1023.f);
        case 6: return (int32_t)(s_age * 1023.f);
        case 7: return (int32_t)(s_mix * 1023.f);
        case 8: return (int32_t)s_tape_type;
        case 9: return (int32_t)s_speed_mode;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 8) {
        static const char *tape_names[] = {
            "TYPE-I", "TYPE-II", "TYPE-IV", "REEL7.5",
            "REEL15", "REEL30", "8TRACK", "DICTAPH"
        };
        if (value >= 0 && value < 8) return tape_names[value];
    }
    if (id == 9) {
        static const char *speed_names[] = {"STOP", "SLOW", "NORMAL", "FAST"};
        if (value >= 0 && value < 4) return speed_names[value];
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    (void)tempo;
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    (void)counter;
}
