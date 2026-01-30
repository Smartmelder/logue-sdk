/*
 
    Copyright (c) 2023, KORG INC.
    
    GABBER BASS V2 - Hardcore gabber oscillator with 10 parameters
*/

#include "unit_osc.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "macros.h"
#include <math.h>

static const unit_runtime_osc_context_t *s_context;

static float s_phase_main = 0.f;
static float s_phase_sub = 0.f;
static float s_phase_fm = 0.f;
static float s_phase_detune[5] = {0.f};
static float s_pitch_env = 0.f;
static uint32_t s_env_counter = 0;

// ORIGINAL 6 PARAMS
static float s_distortion = 0.5f;
static int s_mode = 0;
static float s_pitch_env_amt = 0.75f;
static float s_sub_level = 0.5f;
static float s_detune = 0.5f;
static float s_cutoff = 0.75f;

// NEW 4 PARAMS
static float s_crush = 0.f;
static float s_drive = 0.4f;
static float s_resonance = 0.6f;
static float s_punch = 0.3f;

// NEW STATE
static float s_crush_hold = 0.f;
static uint32_t s_crush_counter = 0;
static float s_filter_z1 = 0.f;
static float s_filter_z2 = 0.f;

#define PITCH_ENV_SAMPLES 2400
#define FM_RATIO 3.0f

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

inline float distort(float x, float amt) {
  if (amt < 0.01f) return x;
  x *= (1.f + amt * 5.f);
  if (x > 1.f) x = 1.f;
  else if (x < -1.f) x = -1.f;
  else x = 1.5f * x - 0.5f * x * x * x;
  x = clipminmaxf(-1.f, x, 1.f);
  return x;
}

static float s_filter_z = 0.f;

inline float filter_lp(float x, float cutoff) {
  float coeff = 0.01f + cutoff * 0.98f;
  s_filter_z = s_filter_z * (1.f - coeff) + x * coeff;
  return s_filter_z;
}

// NEW: BIT CRUSHER
inline float bit_crush(float x, float amount) {
  if (amount < 0.01f) return x;
  
  float bits = 16.f - amount * 14.f;
  float steps = powf(2.f, bits);
  float crushed = floorf(x * steps + 0.5f) / steps;
  
  uint32_t reduction = 1 + (uint32_t)(amount * 15.f);
  
  if (s_crush_counter >= reduction) {
    s_crush_counter = 0;
    s_crush_hold = crushed;
  }
  s_crush_counter++;
  
  return s_crush_hold;
}

// NEW: RESONANT FILTER
inline float filter_resonant(float x, float cutoff, float res) {
  float freq = 20.f + cutoff * 19980.f;
  float w = freq / 48000.f;
  w = clipminmaxf(0.001f, w, 0.499f);
  
  float r = 1.f - res * 0.95f;
  r = clipminmaxf(0.01f, r, 0.999f);
  
  float cos_w = cosf(w * 2.f * 3.14159265f);
  float k = (1.f - 2.f * r * cos_w + r * r) / (2.f - 2.f * cos_w);
  k = clipminmaxf(0.f, k, 1.f);
  
  float a0 = 1.f - k;
  float a1 = 2.f * (k - r) * cos_w;
  float a2 = r * r - k;
  float b1 = 2.f * r * cos_w;
  float b2 = -r * r;
  
  float out = a0 * x + a1 * s_filter_z1 + a2 * s_filter_z2 - b1 * s_filter_z1 - b2 * s_filter_z2;
  
  s_filter_z2 = s_filter_z1;
  s_filter_z1 = x;
  
  if (fabsf(s_filter_z1) < 1e-15f) s_filter_z1 = 0.f;
  if (fabsf(s_filter_z2) < 1e-15f) s_filter_z2 = 0.f;
  
  return clipminmaxf(-2.f, out, 2.f);
}

// NEW: OVERDRIVE
inline float overdrive(float x, float amount) {
  if (amount < 0.01f) return x;
  
  float gain = 1.f + amount * 4.f;
  x *= gain;
  
  // Fast tanh approximation
  x *= 0.8f;
  float x2 = x * x;
  return x * (27.f + x2) / (27.f + 9.f * x2);
}

// NEW: PUNCH ENVELOPE
inline float get_punch_env() {
  if (s_env_counter > PITCH_ENV_SAMPLES) return 0.f;
  
  float t = (float)s_env_counter / 400.f;
  if (t > 1.f) return 0.f;
  
  return (1.f - t) * (1.f - t) * s_punch;
}

#define q31_to_f32_c 4.65661287307739e-010f
#define q31_to_f32(q) ((float)(q) * q31_to_f32_c)

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
  if (!desc)
    return k_unit_err_undef;

  if (desc->target != unit_header.target)
    return k_unit_err_target;

  if (!UNIT_API_IS_COMPAT(desc->api))
    return k_unit_err_api_version;

  if (desc->samplerate != 48000)
    return k_unit_err_samplerate;

  if (desc->input_channels != 2 || desc->output_channels != 1)
    return k_unit_err_geometry;

  s_context = static_cast<const unit_runtime_osc_context_t *>(desc->hooks.runtime_context);

  s_phase_main = 0.f;
  s_phase_sub = 0.f;
  s_phase_fm = 0.f;
  for (int i = 0; i < 5; i++) s_phase_detune[i] = 0.f;
  
  s_pitch_env = 0.f;
  s_env_counter = 0;
  s_filter_z = 0.f;
  s_filter_z1 = 0.f;
  s_filter_z2 = 0.f;
  
  s_crush_hold = 0.f;
  s_crush_counter = 0;

  return k_unit_err_none;
}

__unit_callback void unit_teardown()
{
}

__unit_callback void unit_reset()
{
  s_phase_main = 0.f;
  s_phase_sub = 0.f;
  s_phase_fm = 0.f;
  for (int i = 0; i < 5; i++) s_phase_detune[i] = 0.f;
  s_pitch_env = 0.f;
  s_env_counter = 0;
  s_filter_z = 0.f;
  s_filter_z1 = 0.f;
  s_filter_z2 = 0.f;
  s_crush_hold = 0.f;
  s_crush_counter = 0;
}

__unit_callback void unit_resume()
{
}

__unit_callback void unit_suspend()
{
}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
  const float w0_base = osc_w0f_for_note((s_context->pitch >> 8) & 0xFF, s_context->pitch & 0xFF);
  
  for (uint32_t i = 0; i < frames; i++) {
    float sig = 0.f;
    
    // PITCH ENVELOPE
    if (s_env_counter < PITCH_ENV_SAMPLES) {
      float t = (float)s_env_counter / PITCH_ENV_SAMPLES;
      s_pitch_env = (1.f - t) * (1.f - t);
      s_env_counter++;
    } else {
      s_pitch_env = 0.f;
    }
    
    float pitch_mod = 1.f + s_pitch_env * s_pitch_env_amt * 3.f;
    float w0 = w0_base * pitch_mod;
    
    // WAVEFORM GENERATION
    switch (s_mode) {
      case 0: { // DONK
        float mod = osc_sinf(s_phase_fm);
        float mod_index = 5.f + s_distortion * 30.f;
        float phase_mod = s_phase_main + mod * mod_index * w0;
        sig = 2.f * fmodf(phase_mod, 1.f) - 1.f;
        sig -= poly_blep(fmodf(phase_mod, 1.f), w0);
        s_phase_fm += w0 * FM_RATIO;
        s_phase_fm -= (uint32_t)s_phase_fm;
        break;
      }
      
      case 1: { // HOOVR
        sig = 0.f;
        const float detune_amt = s_detune * 0.1f;
        const float detune_offsets[5] = {-2.f, -1.f, 0.f, 1.f, 2.f};
        
        for (int v = 0; v < 5; v++) {
          float detune = 1.f + detune_offsets[v] * detune_amt;
          float w = w0 * detune;
          float p = s_phase_detune[v];
          float saw = 2.f * p - 1.f;
          saw -= poly_blep(p, w);
          sig += saw * 0.2f;
          s_phase_detune[v] += w;
          s_phase_detune[v] -= (uint32_t)s_phase_detune[v];
        }
        break;
      }
      
      case 2: { // ACID
        sig = 2.f * s_phase_main - 1.f;
        sig -= poly_blep(s_phase_main, w0);
        sig = filter_lp(sig, s_cutoff);
        break;
      }
      
      case 3: { // KICK
        sig = osc_sinf(s_phase_main);
        break;
      }
      
      case 4: { // REESE
        float saw1 = 2.f * s_phase_main - 1.f;
        saw1 -= poly_blep(s_phase_main, w0);
        float detune = 1.f + s_detune * 0.02f;
        float w_det = w0 * detune;
        float p_det = s_phase_detune[0];
        float saw2 = 2.f * p_det - 1.f;
        saw2 -= poly_blep(p_det, w_det);
        sig = (saw1 + saw2) * 0.5f;
        s_phase_detune[0] += w_det;
        s_phase_detune[0] -= (uint32_t)s_phase_detune[0];
        break;
      }
      
      case 5: { // PULSE
        float pw = 0.1f + s_detune * 0.8f;
        sig = (s_phase_main < pw) ? 1.f : -1.f;
        sig += poly_blep(s_phase_main, w0);
        sig -= poly_blep(fmodf(s_phase_main + (1.f - pw), 1.f), w0);
        break;
      }
      
      case 6: { // NOISE
        static uint32_t noise_seed = 1;
        noise_seed = noise_seed * 1103515245 + 12345;
        sig = ((float)(noise_seed >> 16) / 32768.f) - 1.f;
        sig = filter_lp(sig, s_cutoff);
        break;
      }
      
      case 7: { // SUB
        sig = osc_sinf(s_phase_main);
        w0 *= 0.5f;
        break;
      }
    }
    
    // SUB OSCILLATOR
    if (s_sub_level > 0.01f) {
      float sub_sig = osc_sinf(s_phase_sub);
      sig += sub_sig * s_sub_level;
      s_phase_sub += w0 * 0.5f;
      s_phase_sub -= (uint32_t)s_phase_sub;
    }
    
    // NEW: BIT CRUSHER
    if (s_crush > 0.01f) {
      sig = bit_crush(sig, s_crush);
    }
    
    // DISTORTION
    sig = distort(sig, s_distortion);
    
    // NEW: OVERDRIVE
    sig = overdrive(sig, s_drive);
    
    // NEW: RESONANT FILTER
    sig = filter_resonant(sig, s_cutoff, s_resonance);
    
    // NEW: PUNCH
    float punch_env = get_punch_env();
    sig *= (1.f + punch_env);
    
    // PHASE UPDATE
    s_phase_main += w0;
    s_phase_main -= (uint32_t)s_phase_main;
    
    // OUTPUT
    out[i] = clipminmaxf(-1.f, sig * 0.9f, 1.f);
  }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
  value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
  const float valf = param_val_to_f32(value);
  
  switch (id) {
    case 0: s_distortion = valf; break;
    case 1: s_mode = (int)(valf * 7.99f); break;
    case 2: s_pitch_env_amt = valf; break;
    case 3: s_sub_level = valf; break;
    case 4: s_detune = valf; break;
    case 5: s_cutoff = valf; break;
    case 6: s_crush = valf; break;
    case 7: s_drive = valf; break;
    case 8: s_resonance = valf; break;
    case 9: s_punch = valf; break;
    default: break;
  }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
  switch (id) {
    case 0: return (int32_t)(s_distortion * 1023.f);
    case 1: return (int32_t)s_mode;
    case 2: return (int32_t)(s_pitch_env_amt * 1023.f);
    case 3: return (int32_t)(s_sub_level * 1023.f);
    case 4: return (int32_t)(s_detune * 1023.f);
    case 5: return (int32_t)(s_cutoff * 1023.f);
    case 6: return (int32_t)(s_crush * 1023.f);
    case 7: return (int32_t)(s_drive * 1023.f);
    case 8: return (int32_t)(s_resonance * 1023.f);
    case 9: return (int32_t)(s_punch * 1023.f);
    default: return 0;
  }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
  static const char *mode_names[8] = {
    "DONK", "HOOVR", "ACID", "KICK", "REESE", "PULSE", "NOISE", "SUB"
  };
  
  if (id == 1) {
    int mode = (int)((float)value / 1023.f * 7.99f);
    if (mode >= 0 && mode < 8) {
      return mode_names[mode];
    }
  }
  
  return "";
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velo)
{
  s_pitch_env = 1.f;
  s_env_counter = 0;
  s_phase_main = 0.f;
  s_phase_sub = 0.f;
  s_phase_fm = 0.f;
  for (int i = 0; i < 5; i++) s_phase_detune[i] = 0.f;
}

__unit_callback void unit_note_off(uint8_t note)
{
}

__unit_callback void unit_all_note_off()
{
}

__unit_callback void unit_set_tempo(uint32_t tempo)
{
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter)
{
}

__unit_callback void unit_pitch_bend(uint16_t bend)
{
}

__unit_callback void unit_channel_pressure(uint8_t press)
{
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t press)
{
}
