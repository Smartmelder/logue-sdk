/*
    HYPER-CHORD ENGINE voor Korg NTS-1 MKII
    Platform: NTS-1 MKII (logue-sdk)
*/

#include "userosc.h"
#include <math.h>

// Aantal stemmen
#define VOICES 3

typedef struct {
  float phase[VOICES];
  float drive;
  int chord_type;
  float sub_mix;
} State;

static State s_state;

// Akkoord definities (Ratio t.o.v. grondtoon)
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

// Simpele anti-aliasing helper (PolyBLEP)
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

// Lineaire interpolatie helper
inline float linintf(float t, float a, float b) {
  return a + t * (b - a);
}

void OSC_INIT(uint32_t platform, uint32_t api)
{
  for(int i=0; i<VOICES; i++) s_state.phase[i] = 0.f;
  s_state.drive = 0.f;
  s_state.chord_type = 0;
  s_state.sub_mix = 0.5f;
}

void OSC_CYCLE(const user_osc_param_t * const params,
               int32_t *yn,
               const uint32_t frames)
{
  float* phase = s_state.phase;
  const float drive = s_state.drive;
  const int chord = s_state.chord_type;
  const float sub_vol = s_state.sub_mix;
  
  // Noteer: pitch is in 8-bit upper / 8-bit lower format
  const float w0_base = osc_w0f_for_note((params->pitch) >> 8, params->pitch & 0xFF);
  
  q31_t * __restrict y = (q31_t *)yn;

  for (uint32_t i = 0; i < frames; i++) {
    float sig = 0.f;
    
    for (int v = 0; v < VOICES; v++) {
      float ratio = chord_ratios[chord][v];
      
      // Detune effect op stem 2 en 3
      if (drive > 0.1f && v > 0) {
         float detune = (v == 1) ? 1.005f * drive : 0.995f * drive;
         ratio *= (1.0f + (detune * 0.05f)); 
      }

      float w0 = w0_base * ratio;
      float p = phase[v];
      
      // Oscillator vormen
      float raw_saw = (2.f * p - 1.f);
      raw_saw -= poly_blep(p, w0);
      
      float raw_pulse = (p < 0.5f ? 1.f : -1.f);
      raw_pulse += poly_blep(p, w0);
      raw_pulse -= poly_blep(fmodf(p + 0.5f, 1.f), w0);

      // Mix (Drive bepaalt vorm: Square -> Saw)
      float voice_sig = linintf(drive, raw_pulse, raw_saw);

      if (v == 2) voice_sig *= sub_vol;
      
      sig += voice_sig;

      phase[v] += w0;
      phase[v] -= (uint32_t)phase[v];
    }
    
    sig *= 0.33f; // Gain correctie
    y[i] = f32_to_q31(sig);
  }
}

void OSC_NOTEON(const user_osc_param_t * const params)
{
  // Fase reset voor punch
  s_state.phase[0] = 0.f;
  s_state.phase[1] = 0.f;
  s_state.phase[2] = 0.f;
}

void OSC_NOTEOFF(const user_osc_param_t * const params)
{
  (void)params;
}

void OSC_PARAM(uint16_t index, uint16_t value)
{
  const float valf = param_val_to_f32(value);

  switch (index) {
  case k_user_osc_param_id1: // Knop A
    s_state.drive = valf; 
    break;
  case k_user_osc_param_id2: // Knop B
    s_state.chord_type = (int)(valf * 7.99f);
    break;
  case k_user_osc_param_id3: // Parameter 1
    s_state.sub_mix = valf;
    break;
  default: break;
  }
}