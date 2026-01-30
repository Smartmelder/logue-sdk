/*
    VULVA_VET - Compact Plate Reverb
    
    Memory-optimized Dattorro topology for NTS-1 mkII
    Total memory: ~24KB (safe for modfx)
*/

#include "unit_revfx.h"
#include "osc_api.h"
#include "fx_api.h"
#include "utils/float_math.h"

// ========== ULTRA-COMPACT DELAY SIZES (Total: ~12KB) ==========

#define PREDELAY_SIZE 600         // 12.5ms
#define DIFF1_SIZE 19
#define DIFF2_SIZE 23
#define DIFF3_SIZE 31
#define DIFF4_SIZE 37

#define TANK_L1_SIZE 557
#define TANK_L2_SIZE 463
#define TANK_L3_SIZE 359
#define TANK_L4_SIZE 251

#define TANK_R1_SIZE 533
#define TANK_R2_SIZE 443
#define TANK_R3_SIZE 347
#define TANK_R4_SIZE 233

// ========== BUFFERS ==========

static float s_pre[PREDELAY_SIZE];
static float s_d1[DIFF1_SIZE], s_d2[DIFF2_SIZE], s_d3[DIFF3_SIZE], s_d4[DIFF4_SIZE];
static float s_tl1[TANK_L1_SIZE], s_tl2[TANK_L2_SIZE], s_tl3[TANK_L3_SIZE], s_tl4[TANK_L4_SIZE];
static float s_tr1[TANK_R1_SIZE], s_tr2[TANK_R2_SIZE], s_tr3[TANK_R3_SIZE], s_tr4[TANK_R4_SIZE];

static uint32_t s_pw = 0, s_d1w = 0, s_d2w = 0, s_d3w = 0, s_d4w = 0;
static uint32_t s_tl1w = 0, s_tl2w = 0, s_tl3w = 0, s_tl4w = 0;
static uint32_t s_tr1w = 0, s_tr2w = 0, s_tr3w = 0, s_tr4w = 0;

static float s_lp_l = 0.f, s_lp_r = 0.f;
static float s_lfo = 0.f;

static float s_time = 0.6f, s_damp = 0.4f, s_mix = 0.75f;

// ========== HELPERS ==========

inline float read_interp(const float* buf, uint32_t size, uint32_t w, float d) {
    d = clipminmaxf(1.f, d, (float)(size - 1));
    float r = (float)w - d;
    while (r < 0.f) r += (float)size;
    while (r >= (float)size) r -= (float)size;
    
    uint32_t i0 = (uint32_t)r;
    uint32_t i1 = (i0 + 1) % size;
    float frac = r - (float)i0;
    
    return buf[i0] * (1.f - frac) + buf[i1] * frac;
}

inline float allpass(float in, float* buf, uint32_t size, uint32_t* w, float g) {
    uint32_t r = (*w + size - 1) % size;
    float delayed = buf[r];
    float out = -in + delayed;
    buf[*w] = in + delayed * g;
    *w = (*w + 1) % size;
    return out;
}

// ========== PROCESS ==========

inline void process_reverb(float il, float ir, float* ol, float* out_r) {
    float mono = (il + ir) * 0.5f;
    
    // Predelay
    s_pre[s_pw] = mono;
    float pre_out = s_pre[(s_pw + PREDELAY_SIZE - (PREDELAY_SIZE / 4)) % PREDELAY_SIZE];
    s_pw = (s_pw + 1) % PREDELAY_SIZE;
    
    // Diffusion
    float d = pre_out;
    d = allpass(d, s_d1, DIFF1_SIZE, &s_d1w, 0.75f);
    d = allpass(d, s_d2, DIFF2_SIZE, &s_d2w, 0.75f);
    d = allpass(d, s_d3, DIFF3_SIZE, &s_d3w, 0.625f);
    d = allpass(d, s_d4, DIFF4_SIZE, &s_d4w, 0.625f);
    
    // LFO
    s_lfo += 0.7f / 48000.f;
    if (s_lfo >= 1.f) s_lfo -= 1.f;
    float lfo_val = osc_sinf(s_lfo);
    float mod = lfo_val * 15.f;
    
    // Feedback
    float fb = 0.1f + s_time * 0.85f;
    fb = clipminmaxf(0.1f, fb, 0.95f);
    
    // Tank L
    float tl_out = s_tl1[(s_tl1w + TANK_L1_SIZE - 131) % TANK_L1_SIZE];
    float tl_tap = s_tl2[(s_tl2w + TANK_L2_SIZE - 467) % TANK_L2_SIZE];
    
    float in_l = d + s_tr4[(s_tr4w + TANK_R4_SIZE - 1) % TANK_R4_SIZE] * fb;
    float dl1 = read_interp(s_tl1, TANK_L1_SIZE, s_tl1w, (float)(TANK_L1_SIZE - 5) + mod);
    s_tl1[s_tl1w] = in_l; s_tl1w = (s_tl1w + 1) % TANK_L1_SIZE;
    
    float dl2 = allpass(dl1, s_tl3, TANK_L3_SIZE, &s_tl3w, 0.5f);
    s_tl2[s_tl2w] = dl2;
    float dl3 = s_tl2[(s_tl2w + TANK_L2_SIZE - 1) % TANK_L2_SIZE];
    s_tl2w = (s_tl2w + 1) % TANK_L2_SIZE;
    
    // Damping
    float damp_coeff = 1.f - s_damp;
    s_lp_l = s_lp_l * damp_coeff + dl3 * (1.f - damp_coeff);
    if (si_fabsf(s_lp_l) < 1e-15f) s_lp_l = 0.f;
    
    s_tl4[s_tl4w] = s_lp_l; s_tl4w = (s_tl4w + 1) % TANK_L4_SIZE;
    
    // Tank R
    float tr_out = s_tr1[(s_tr1w + TANK_R1_SIZE - 151) % TANK_R1_SIZE];
    float tr_tap = s_tr2[(s_tr2w + TANK_R2_SIZE - 89) % TANK_R2_SIZE];
    
    float in_r = d + s_tl4[(s_tl4w + TANK_L4_SIZE - 1) % TANK_L4_SIZE] * fb;
    float dr1 = read_interp(s_tr1, TANK_R1_SIZE, s_tr1w, (float)(TANK_R1_SIZE - 5) - mod);
    s_tr1[s_tr1w] = in_r; s_tr1w = (s_tr1w + 1) % TANK_R1_SIZE;
    
    float dr2 = allpass(dr1, s_tr3, TANK_R3_SIZE, &s_tr3w, 0.5f);
    s_tr2[s_tr2w] = dr2;
    float dr3 = s_tr2[(s_tr2w + TANK_R2_SIZE - 1) % TANK_R2_SIZE];
    s_tr2w = (s_tr2w + 1) % TANK_R2_SIZE;
    
    s_lp_r = s_lp_r * damp_coeff + dr3 * (1.f - damp_coeff);
    if (si_fabsf(s_lp_r) < 1e-15f) s_lp_r = 0.f;
    
    s_tr4[s_tr4w] = s_lp_r; s_tr4w = (s_tr4w + 1) % TANK_R4_SIZE;
    
    // Output
    *ol = tl_out - tr_tap;
    *out_r = tr_out - tl_tap;
    
    *ol = fastertanhf(*ol * 0.8f);
    *out_r = fastertanhf(*out_r * 0.8f);
}

// ========== CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;
    
    // Clear buffers
    for (int i = 0; i < PREDELAY_SIZE; i++) s_pre[i] = 0.f;
    for (int i = 0; i < DIFF1_SIZE; i++) s_d1[i] = 0.f;
    for (int i = 0; i < DIFF2_SIZE; i++) s_d2[i] = 0.f;
    for (int i = 0; i < DIFF3_SIZE; i++) s_d3[i] = 0.f;
    for (int i = 0; i < DIFF4_SIZE; i++) s_d4[i] = 0.f;
    for (int i = 0; i < TANK_L1_SIZE; i++) s_tl1[i] = 0.f;
    for (int i = 0; i < TANK_L2_SIZE; i++) s_tl2[i] = 0.f;
    for (int i = 0; i < TANK_L3_SIZE; i++) s_tl3[i] = 0.f;
    for (int i = 0; i < TANK_L4_SIZE; i++) s_tl4[i] = 0.f;
    for (int i = 0; i < TANK_R1_SIZE; i++) s_tr1[i] = 0.f;
    for (int i = 0; i < TANK_R2_SIZE; i++) s_tr2[i] = 0.f;
    for (int i = 0; i < TANK_R3_SIZE; i++) s_tr3[i] = 0.f;
    for (int i = 0; i < TANK_R4_SIZE; i++) s_tr4[i] = 0.f;
    
    s_pw = s_d1w = s_d2w = s_d3w = s_d4w = 0;
    s_tl1w = s_tl2w = s_tl3w = s_tl4w = 0;
    s_tr1w = s_tr2w = s_tr3w = s_tr4w = 0;
    s_lp_l = s_lp_r = s_lfo = 0.f;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}
__unit_callback void unit_reset() {
    s_lp_l = s_lp_r = s_lfo = 0.f;
}
__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    for (uint32_t f = 0; f < frames; f++) {
        float il = in[f * 2];
        float ir = in[f * 2 + 1];
        
        float wl = 0.f, wr = 0.f;
        process_reverb(il, ir, &wl, &wr);
        
        float dry_g = osc_cosf(s_mix * 1.5707f);
        float wet_g = osc_sinf(s_mix * 1.5707f);
        
        out[f * 2] = il * dry_g + wl * wet_g;
        out[f * 2 + 1] = ir * dry_g + wr * wet_g;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_time = valf; break;
        case 1: s_damp = valf; break;
        case 2: s_mix = valf; break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)(s_time * 1023.f);
        case 1: return (int32_t)(s_damp * 1023.f);
        case 2: return (int32_t)(s_mix * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    (void)id; (void)value;
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) { (void)tempo; }
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) { (void)counter; }
