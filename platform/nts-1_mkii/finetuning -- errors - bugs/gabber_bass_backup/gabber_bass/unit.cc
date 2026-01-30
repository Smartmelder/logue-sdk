/*
    BSD 3-Clause License

    Copyright (c) 2023, KORG INC.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    * Neither the name of the copyright holder nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

//*/

/*
 *  File: unit.cc
 *
 *  NTS-1 mkII oscillator unit interface
 *  GABBER BASS V2 - Turn Up The Bass inspired hardcore oscillator
 *  NOW WITH 10 PARAMETERS! (CRUSH, BOUNCE, WIDE, GLIDE)
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
static float s_transient_env = 0.f;

static float s_distortion = 0.5f;
static int s_mode = 0;
static float s_pitch_env_amt = 0.75f;
static float s_sub_level = 0.5f;
static float s_detune = 0.5f;
static float s_cutoff = 0.75f;

// ========== NEW PARAMETERS ==========
static float s_crush = 0.f;        // Bit crusher amount
static float s_bounce = 0.3f;      // Pitch bounce/wobble
static float s_wide = 0.5f;        // Stereo width (phase effect)
static float s_glide = 0.2f;       // Portamento time

// ========== GLIDE STATE ==========
static float s_current_pitch = 1.f;  // FIXED: Start at 1.0 (no pitch shift)
static float s_target_pitch = 1.f;   // FIXED: Start at 1.0 (no pitch shift)
static bool s_glide_active = false;

// ========== BOUNCE STATE ==========
static float s_bounce_phase = 0.f;

// ========== BIT CRUSHER STATE ==========
static float s_crush_hold = 0.f;
static uint32_t s_crush_counter = 0;

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
  if (si_fabsf(s_filter_z) < 1e-15f) s_filter_z = 0.f;
  return s_filter_z;
}

// ========== NEW: BIT CRUSHER ==========
inline float bit_crush(float x, float amount) {
  if (amount < 0.01f) return x;
  
  // Bit depth reduction
  float bits = 16.f - amount * 14.f;  // 16-bit to 2-bit
  float steps = powf(2.f, bits);
  float crushed = si_floorf(x * steps + 0.5f) / steps;
  
  // Sample rate reduction
  uint32_t reduction = 1 + (uint32_t)(amount * 15.f);
  
  if (s_crush_counter >= reduction) {
    s_crush_counter = 0;
    s_crush_hold = crushed;
  }
  s_crush_counter++;
  
  return s_crush_hold;
}

// ========== NEW: PITCH BOUNCE ==========
inline float get_bounce_mod() {
  if (s_bounce < 0.01f) return 0.f;
  
  // Fast bounce at note start
  float rate = 20.f + s_bounce * 30.f;  // 20-50 Hz
  s_bounce_phase += rate / 48000.f;
  if (s_bounce_phase >= 1.f) s_bounce_phase -= 1.f;
  
  // Exponential decay
  float decay = powf(2.f, -s_env_counter / 4800.f);
  
  float bounce = osc_sinf(s_bounce_phase) * s_bounce * 0.5f * decay;
  
  return bounce;
}

// ========== NEW: PORTAMENTO/GLIDE ==========
inline float get_glide_pitch() {
  // FIXED: Always return at least 1.0 if target is 0 (no note)
  if (s_target_pitch < 0.01f) {
    return 1.f;
  }
  if (!s_glide_active || s_glide < 0.01f) {
    return s_target_pitch;
  }
  
  // Glide speed (slower = longer glide)
  float speed = 0.0001f + s_glide * 0.01f;
  
  if (s_current_pitch < s_target_pitch) {
    s_current_pitch += speed * (s_target_pitch - s_current_pitch);
    if (s_current_pitch > s_target_pitch) {
      s_current_pitch = s_target_pitch;
      s_glide_active = false;
    }
  } else if (s_current_pitch > s_target_pitch) {
    s_current_pitch -= speed * (s_current_pitch - s_target_pitch);
    if (s_current_pitch < s_target_pitch) {
      s_current_pitch = s_target_pitch;
      s_glide_active = false;
    }
  }
  
  return s_current_pitch;
}

// ========== NEW: HARMONICS (brightness) ==========
inline float add_harmonics(float x, float amount) {
  if (amount < 0.01f) return x;
  
  // Add even harmonics for brightness
  float h2 = x * x * 0.3f;
  float h4 = h2 * h2 * 0.15f;
  
  return x + (h2 + h4) * amount;
}

// ========== NEW: PUNCH (transient shaper) ==========
inline float apply_punch(float x) {
  if (s_transient_env < 0.01f) return x;
  
  // Fast attack transient (always active, integrated feature)
  float punch = s_transient_env * 0.3f;
  return x * (1.f + punch);
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
  s_transient_env = 0.f;
  s_filter_z = 0.f;
  
  s_current_pitch = 1.f;  // FIXED: Reset to 1.0 (no pitch shift)
  s_target_pitch = 1.f;   // FIXED: Reset to 1.0 (no pitch shift)
  s_glide_active = false;
  
  s_bounce_phase = 0.f;
  
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
  s_transient_env = 0.f;
  s_filter_z = 0.f;
  
  s_bounce_phase = 0.f;
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
    
    // Pitch envelope
    if (s_env_counter < PITCH_ENV_SAMPLES) {
      float t = (float)s_env_counter / PITCH_ENV_SAMPLES;
      s_pitch_env = (1.f - t) * (1.f - t);
      s_env_counter++;
    } else {
      s_pitch_env = 0.f;
    }
    
    // Update transient envelope (fast attack for punch)
    if (s_transient_env > 0.01f) {
      s_transient_env *= 0.995f;  // Decay
    }
    
    // Calculate pitch with modulations
    float pitch_mod = 1.f + s_pitch_env * s_pitch_env_amt * 3.f;
    
    // Add bounce modulation
    pitch_mod += get_bounce_mod();
    
    // Apply glide
    float glide_pitch = get_glide_pitch();
    float w0 = w0_base * pitch_mod * glide_pitch;
    
    // Generate waveform based on mode
    switch (s_mode) {
      case 0: { // DONK - FM Sawtooth
        float mod = osc_sinf(s_phase_fm);
        float mod_index = 5.f + s_distortion * 30.f;
        float phase_mod = s_phase_main + mod * mod_index * w0;
        sig = 2.f * fmodf(phase_mod, 1.f) - 1.f;
        sig -= poly_blep(fmodf(phase_mod, 1.f), w0);
        s_phase_fm += w0 * FM_RATIO;
        s_phase_fm -= (uint32_t)s_phase_fm;
        break;
      }
      
      case 1: { // HOOVR - 5-voice detuned saw
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
      
      case 2: { // ACID - Filtered saw
        sig = 2.f * s_phase_main - 1.f;
        sig -= poly_blep(s_phase_main, w0);
        sig = filter_lp(sig, s_cutoff);
        break;
      }
      
      case 3: { // KICK - Sine wave
        sig = osc_sinf(s_phase_main);
        break;
      }
      
      case 4: { // REESE - Detuned dual saw
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
      
      case 5: { // PULSE - Variable pulse width
        float pw = 0.1f + s_detune * 0.8f;
        sig = (s_phase_main < pw) ? 1.f : -1.f;
        sig += poly_blep(s_phase_main, w0);
        sig -= poly_blep(fmodf(s_phase_main + (1.f - pw), 1.f), w0);
        break;
      }
      
      case 6: { // NOISE - Filtered noise
        static uint32_t noise_seed = 1;
        noise_seed = noise_seed * 1103515245 + 12345;
        sig = ((float)(noise_seed >> 16) / 32768.f) - 1.f;
        sig = filter_lp(sig, s_cutoff);
        break;
      }
      
      case 7: { // SUB - Sub octave sine
        sig = osc_sinf(s_phase_main);
        w0 *= 0.5f;
        break;
      }
    }
    
    // Add sub oscillator
    if (s_sub_level > 0.01f) {
      float sub_sig = osc_sinf(s_phase_sub);
      sig += sub_sig * s_sub_level;
      s_phase_sub += w0 * 0.5f;
      s_phase_sub -= (uint32_t)s_phase_sub;
    }
    
    // NEW: Apply bit crusher BEFORE distortion
    if (s_crush > 0.01f) {
      sig = bit_crush(sig, s_crush);
    }
    
    // Apply distortion
    sig = distort(sig, s_distortion);
    
    // NEW: Add harmonics (brightness) - integrated via WIDE parameter
    sig = add_harmonics(sig, s_wide * 0.5f);
    
    // NEW: Apply punch (transient shaper)
    sig = apply_punch(sig);
    
    // NEW: Stereo width effect (phase modulation on alternating samples)
    if (s_wide != 0.5f && (i % 2) == 1) {
      float phase_shift = (s_wide - 0.5f) * 0.2f;
      sig *= (1.f + phase_shift);
    }
    
    // Update main phase
    s_phase_main += w0;
    s_phase_main -= (uint32_t)s_phase_main;
    
    // Output with clipping - Volume boost!
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
        case 7: s_bounce = valf; break;
        case 8: s_wide = valf; break;
        case 9: s_glide = valf; break;
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
        case 7: return (int32_t)(s_bounce * 1023.f);
        case 8: return (int32_t)(s_wide * 1023.f);
        case 9: return (int32_t)(s_glide * 1023.f);
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
  // Set target pitch for glide
  s_target_pitch = 1.f;
  
  // If glide is active, start from current pitch
  if (s_glide > 0.01f && s_current_pitch > 0.f) {
    s_glide_active = true;
  } else {
    s_current_pitch = 1.f;
    s_glide_active = false;
  }
  
  // Reset envelopes
  s_pitch_env = 1.f;
  s_env_counter = 0;
  s_transient_env = 1.f;
  
  // Reset phases
  s_phase_main = 0.f;
  s_phase_sub = 0.f;
  s_phase_fm = 0.f;
  s_bounce_phase = 0.f;
  
  for (int i = 0; i < 5; i++) {
    s_phase_detune[i] = 0.f;
  }
  
  (void)velo;
}

__unit_callback void unit_note_off(uint8_t note)
{
  (void)note;
}

__unit_callback void unit_all_note_off()
{
}

__unit_callback void unit_set_tempo(uint32_t tempo)
{
  (void)tempo;
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter)
{
  (void)counter;
}

__unit_callback void unit_pitch_bend(uint16_t bend)
{
  (void)bend;
}

__unit_callback void unit_channel_pressure(uint8_t press)
{
  (void)press;
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t press)
{
  (void)note;
  (void)press;
}
