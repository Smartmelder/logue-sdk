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
 *  NTS-1 mkII oscillator unit interface - Hyper Chord Engine
 *
 */

#include "unit_osc.h"       // base definitions for osc units
#include "utils/float_math.h" // clipminmaxf, linintf
#include "utils/int_math.h"  // clipminmaxi32
#include "macros.h"          // param_val_to_f32

// State variables
static float s_phase[3] = {0.f, 0.f, 0.f};  // Phase for 3 voices
static float s_shape = 0.f;                  // Shape/Detune parameter (0-1)
static int s_chord_type = 0;                 // Chord type (0-7)
static float s_sub_mix = 0.5f;               // Sub mix (0-1)

static const unit_runtime_osc_context_t *s_context;

// Chord ratios relative to root
const float chord_ratios[8][3] = {
  {1.0f, 1.00f, 0.50f}, // 0: Mono/Unison
  {1.0f, 2.00f, 0.50f}, // 1: Octave
  {1.0f, 1.50f, 0.50f}, // 2: 5th (Power Chord)
  {1.0f, 1.26f, 1.50f}, // 3: Major
  {1.0f, 1.19f, 1.50f}, // 4: Minor
  {1.0f, 1.33f, 1.50f}, // 5: Sus4
  {1.0f, 1.26f, 1.41f}, // 6: Dom7
  {1.0f, 1.50f, 3.00f}  // 7: Rave
};

// Simple anti-aliasing helper (PolyBLEP)
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

// from fixed_math.h
#define q31_to_f32_c 4.65661287307739e-010f
#define q31_to_f32(q) ((float)(q) * q31_to_f32_c)

// ---- Callbacks exposed to runtime ----------------------------------------------

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
  if (!desc)
    return k_unit_err_undef;

  // Note: make sure the unit is being loaded to the correct platform/module target
  if (desc->target != unit_header.target)
    return k_unit_err_target;

  // Note: check API compatibility with the one this unit was built against
  if (!UNIT_API_IS_COMPAT(desc->api))
    return k_unit_err_api_version;

  // Check compatibility of samplerate with unit
  if (desc->samplerate != 48000)
    return k_unit_err_samplerate;

  // Check compatibility of frame geometry
  // note: NTS-1 mkII oscillators can make use of the audio input depending on the routing options in global settings
  if (desc->input_channels != 2 || desc->output_channels != 1) // should be stereo input / mono output
    return k_unit_err_geometry;

  // cache the context for later use
  s_context = static_cast<const unit_runtime_osc_context_t *>(desc->hooks.runtime_context);

  // Initialize state
  for (int i = 0; i < 3; i++) {
    s_phase[i] = 0.f;
  }
  s_shape = 0.f;
  s_chord_type = 0;
  s_sub_mix = 0.5f;

  return k_unit_err_none;
}

__unit_callback void unit_teardown()
{
  // Cleanup if needed
}

__unit_callback void unit_reset()
{
  // Reset state
  for (int i = 0; i < 3; i++) {
    s_phase[i] = 0.f;
  }
}

__unit_callback void unit_resume()
{
  // Resume from suspend
}

__unit_callback void unit_suspend()
{
  // Suspend rendering
}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
  // CRITICAL: This function MUST generate audio and write to out[]
  
  // Get pitch from context
  const float w0_base = osc_w0f_for_note((s_context->pitch >> 8) & 0xFF, s_context->pitch & 0xFF);
  
  // Process each frame
  for (uint32_t i = 0; i < frames; i++) {
    float sig = 0.f;
    
    // Generate 3 voices for chord
    for (int v = 0; v < 3; v++) {
      float ratio = chord_ratios[s_chord_type][v];
      
      // Detune effect on voice 2 and 3
      if (s_shape > 0.1f && v > 0) {
        float detune = (v == 1) ? 1.005f * s_shape : 0.995f * s_shape;
        ratio *= (1.0f + (detune * 0.05f));
      }
      
      float w0 = w0_base * ratio;
      float p = s_phase[v];
      
      // Generate sawtooth waveform
      float raw_saw = (2.f * p - 1.f);
      raw_saw -= poly_blep(p, w0);
      
      // Generate pulse waveform
      float raw_pulse = (p < 0.5f ? 1.f : -1.f);
      raw_pulse += poly_blep(p, w0);
      raw_pulse -= poly_blep(fmodf(p + 0.5f, 1.f), w0);
      
      // Mix between pulse and saw (Shape parameter controls this)
      float voice_sig = linintf(s_shape, raw_pulse, raw_saw);
      
      // Apply sub mix to voice 2 (sub oscillator)
      if (v == 2) {
        voice_sig *= s_sub_mix;
      }
      
      sig += voice_sig;
      
      // Update phase
      s_phase[v] += w0;
      s_phase[v] -= (uint32_t)s_phase[v];  // Wrap to [0, 1)
    }
    
    // Normalize and clip output
    sig *= 0.33f;  // Gain correction for 3 voices
    out[i] = clipminmaxf(-1.f, sig, 1.f);  // CRITICAL: Always write to out[i] and clip!
  }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
  // Clip to valid range as defined in header
  value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
  
  // Convert to float and update state
  const float valf = param_val_to_f32(value);
  
  switch (id) {
    case 0:  // Shape/Detune
      s_shape = valf;
      break;
    case 1:  // Chord Select
      s_chord_type = (int)(valf * 7.99f);
      break;
    case 2:  // Sub Mix
      s_sub_mix = valf;
      break;
    default:
      break;
  }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
  // Return current parameter value
  switch (id) {
    case 0:
      return (int32_t)(s_shape * 1023.f);
    case 1:
      return (int32_t)s_chord_type;
    case 2:
      return (int32_t)(s_sub_mix * 1023.f);
    default:
      return 0;
  }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
  // Return empty string for numeric display
  return "";
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velo)
{
  // Reset phases for punch
  for (int i = 0; i < 3; i++) {
    s_phase[i] = 0.f;
  }
}

__unit_callback void unit_note_off(uint8_t note)
{
  // Note off handling (if needed)
}

__unit_callback void unit_all_note_off()
{
  // All notes off
}

__unit_callback void unit_set_tempo(uint32_t tempo)
{
  // Tempo setting (if needed)
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter)
{
  // Tempo tick (if needed)
}

__unit_callback void unit_pitch_bend(uint16_t bend)
{
  // Pitch bend (if needed)
}

__unit_callback void unit_channel_pressure(uint8_t press)
{
  // Channel pressure (if needed)
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t press)
{
  // Aftertouch (if needed)
}

