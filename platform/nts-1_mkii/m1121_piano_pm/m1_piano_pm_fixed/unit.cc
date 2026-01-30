/*
    KORG M1 PIANO - HYBRID PHYSICAL MODELING ENGINE

    ARCHITECTURE:

    1. EXCITER (2-OP FM BURST):
       - Carrier: Sine wave
       - Modulator: Sine wave @ 4.2:1 ratio (non-integer!)
       - High FM index (6-12) creates metallic "clang"
       - Ultra-fast envelope (5-20ms decay)
       - Creates iconic M1 attack transient

    2. RESONATOR (EXTENDED KARPLUS-STRONG):
       - Delay line (string length)
       - Stiffness allpass filter (inharmonicity!)
       - Lowpass filter (damping)
       - Feedback control (decay time)

       STIFFNESS ALLPASS:
       - Introduces frequency-dependent delay
       - High frequencies travel faster (dispersion)
       - Creates "stiff wire" character
       - Formula: H(z) = (c + z^-1) / (1 + c*z^-1)

    3. POST-PROCESSING:
       - Comb filter @ 2-3kHz (M1 DAC character)
       - Peaking EQ (body resonance)
       - Stereo chorus
       - Unison detune

    PHYSICAL MODELING THEORY:

    Piano string physics:
    - Inharmonicity factor B = (π³Ed⁴) / (64TL²)
    - Where: E=Young's modulus, d=diameter, T=tension, L=length
    - Higher notes → more inharmonicity
    - M1 samples exhibit strong inharmonicity

    Dispersion relation:
    - f_n = f₀ × n × √(1 + Bn²)
    - Partials are stretched (not perfect harmonics)

    PRESETS:
    0. M1 PIANO - Classic bright M1
    1. HOUSE - 90s house piano stab
    2. RHODES - Mellow, less metallic
    3. TRANCE - Super bright, long decay
    4. BELL - Maximum inharmonicity
    5. WURLI - Wurlitzer character
    6. DETUNED - Wide stereo spread
    7. LOFI - Dark, damped

    BRONNEN:
    - Korg M1 PCM sample analysis
    - Karplus-Strong algorithm (1983)
    - "Physical Audio Signal Processing" (J.O. Smith)
    - Piano string inharmonicity theory
    - Dispersion filters for stiffness modeling
*/

#include "macros.h"
#include "unit_osc.h"
#include "utils/float_math.h"
#include "utils/int_math.h"

// SDK compatibility - NO math.h!
// SDK compatibility - PI is already defined in CMSIS arm_math.h
// const float PI = 3.14159265359f; // Removed - conflicts with CMSIS

#define MAX_VOICES 3
#define MAX_DELAY_LENGTH 512    // Reduced from 2048
#define SINE_TABLE_SIZE 256     // Reduced from 1024
#define CHORUS_BUFFER_SIZE 1024 // Reduced from 4096

static const unit_runtime_osc_context_t *s_context;

// Pre-computed sine table (optimization)
static float s_sine_table[SINE_TABLE_SIZE];

struct DelayLine {
  float buffer[MAX_DELAY_LENGTH];
  uint32_t write_pos;
  uint32_t length;

  // Stiffness allpass state
  float allpass_z1;
  float allpass_coeff;

  // Damping lowpass state
  float lpf_z1;

  // Feedback
  float feedback;
};

struct Voice {
  // Exciter (2-Op FM burst)
  float exciter_phase_carrier;
  float exciter_phase_mod;
  float exciter_env;
  uint32_t exciter_counter;
  bool exciter_active;

  // Resonator (Karplus-Strong + Stiffness)
  DelayLine delay_line[2]; // 2 for unison/detune

  // Body/comb filter
  float comb_buffer[256];
  uint32_t comb_write;

  // Post EQ
  float peak_z1, peak_z2;

  // Release envelope
  float release_env;
  uint8_t release_stage;
  uint32_t release_counter;

  // Voice info
  uint8_t note;
  uint8_t velocity;
  bool active;
};

static Voice s_voices[MAX_VOICES];

// Chorus buffer
static float s_chorus_buffer_l[CHORUS_BUFFER_SIZE];
static float s_chorus_buffer_r[CHORUS_BUFFER_SIZE];
static uint32_t s_chorus_write;
static float s_chorus_lfo_phase;

// Parameters
static float s_hardness;       // FM index + damping cutoff
static float s_decay_time;     // Feedback amount
static float s_stiffness;      // Allpass coefficient
static float s_detune_amount;  // Unison spread
static float s_brightness;     // Tone control
static float s_body_resonance; // Body EQ
static float s_chorus_depth;   // Chorus effect
static float s_release_time;   // Release time
static uint8_t s_preset;
static float s_velocity_sens; // Velocity sensitivity

static uint32_t s_sample_counter;

// Presets
struct M1PianoPreset {
  float hardness;
  float decay;
  float stiffness;
  float detune;
  float brightness;
  float body;
  float chorus;
  float release;
  const char *name;
};

static const M1PianoPreset s_presets[8] = {
    {0.75f, 0.60f, 0.80f, 0.30f, 0.70f, 0.50f, 0.25f, 0.40f, "M1PIANO"},
    {0.85f, 0.55f, 0.75f, 0.35f, 0.85f, 0.60f, 0.40f, 0.30f, "HOUSE"},
    {0.55f, 0.70f, 0.60f, 0.20f, 0.50f, 0.45f, 0.35f, 0.60f, "RHODES"},
    {0.90f, 0.75f, 0.85f, 0.40f, 0.90f, 0.55f, 0.50f, 0.80f, "TRANCE"},
    {0.95f, 0.65f, 0.95f, 0.45f, 0.80f, 0.40f, 0.30f, 0.70f, "BELL"},
    {0.60f, 0.65f, 0.65f, 0.25f, 0.60f, 0.70f, 0.20f, 0.50f, "WURLI"},
    {0.70f, 0.60f, 0.75f, 0.70f, 0.75f, 0.50f, 0.60f, 0.45f, "DETUNE"},
    {0.50f, 0.50f, 0.70f, 0.15f, 0.40f, 0.35f, 0.15f, 0.35f, "LOFI"}};

void init_sine_table() {
  for (int i = 0; i < SINE_TABLE_SIZE; i++) {
    float phase = (float)i / (float)SINE_TABLE_SIZE;
    s_sine_table[i] = osc_sinf(phase);
  }
}

inline float sine_lookup(float phase) {
  phase -= (int32_t)phase;
  if (phase < 0.f)
    phase += 1.f;

  float idx_f = phase * (float)(SINE_TABLE_SIZE - 1);
  uint32_t idx0 = (uint32_t)idx_f;
  uint32_t idx1 = (idx0 + 1) % SINE_TABLE_SIZE;
  float frac = idx_f - (float)idx0;

  return s_sine_table[idx0] * (1.f - frac) + s_sine_table[idx1] * frac;
}

inline float fast_tanh(float x) {
  if (x < -3.f)
    return -1.f;
  if (x > 3.f)
    return 1.f;
  float x2 = x * x;
  return x * (27.f + x2) / (27.f + 9.f * x2);
}

// 2-OP FM EXCITER (Metallic hammer strike)
inline float fm_exciter(Voice *v, float hardness) {
  if (!v->exciter_active)
    return 0.f;

  // Non-integer FM ratio (4.2:1) for metallic character
  float fm_ratio = 4.2f;

  // High FM index for "clang"
  float mod_index = 8.f + hardness * 8.f; // VERHOOGD: 8-16 range

  // 2-OP FM synthesis
  float mod_phase_norm = v->exciter_phase_mod * fm_ratio;
  mod_phase_norm -= (uint32_t)mod_phase_norm;
  if (mod_phase_norm < 0.f)
    mod_phase_norm += 1.f;

  float modulator = sine_lookup(mod_phase_norm);

  float carrier_phase_mod =
      v->exciter_phase_carrier + mod_index * modulator * 0.5f;
  carrier_phase_mod -= (uint32_t)carrier_phase_mod;
  if (carrier_phase_mod < 0.f)
    carrier_phase_mod += 1.f;

  float carrier = sine_lookup(carrier_phase_mod);

  // Ultra-fast decay envelope
  float decay_time = 0.008f + hardness * 0.022f; // VERHOOGD: 8-30ms
  float t_sec = (float)v->exciter_counter / 48000.f;

  if (t_sec < decay_time) {
    v->exciter_env = fastpow2f(-t_sec / decay_time * 6.f); // Exponential
  } else {
    v->exciter_env = 0.f;
    v->exciter_active = false;
  }

  v->exciter_counter++;

  // Apply envelope
  float output = carrier * v->exciter_env;

  // Velocity sensitivity (now parameter-controlled)
  float vel_scale = (float)v->velocity / 127.f;
  float vel_min = 0.3f + s_velocity_sens * 0.5f; // 0.3-0.8 range
  float vel_max = 0.8f + s_velocity_sens * 0.2f; // 0.8-1.0 range
  vel_scale = vel_min + vel_scale * (vel_max - vel_min);
  output *= vel_scale;

  // EXTRA BOOST voor initiële excitatie
  output *= 2.5f;

  return output;
}

// STIFFNESS ALLPASS FILTER (Inharmonicity!)
inline float stiffness_allpass(float input, float *z1, float coeff) {
  // 1st-order allpass: H(z) = (c + z^-1) / (1 + c*z^-1)
  // Creates frequency-dependent phase shift (dispersion)

  float output = coeff * input + *z1;
  *z1 = input - coeff * output;

  return output;
}

// DAMPING LOWPASS FILTER
inline float damping_lpf(float input, float *z1, float cutoff_hz) {
  // Simple 1-pole lowpass for string damping
  // Fixed: cutoff_hz is now in Hz (not normalized)
  float w = 2.f * PI * cutoff_hz / 48000.f;
  // osc_tanpif expects phase in [0.0001, 0.49], so convert w*0.5f to that range
  float phase = (w * 0.5f) / PI;
  if (phase > 0.49f)
    phase = 0.49f;
  if (phase < 0.0001f)
    phase = 0.0001f;
  float g = osc_tanpif(phase);
  g = g / (1.f + g);

  *z1 = *z1 + g * (input - *z1);
  return *z1;
}

// KARPLUS-STRONG RESONATOR with STIFFNESS
inline float karplus_strong_process(DelayLine *dl, float exciter_input,
                                    float stiffness, float damping_cutoff) {
  // Read from delay line
  uint32_t read_pos =
      (dl->write_pos + MAX_DELAY_LENGTH - dl->length) % MAX_DELAY_LENGTH;
  float delayed = dl->buffer[read_pos];

  // Add exciter (hammer strike)
  float input = exciter_input + delayed * dl->feedback;

  // STIFFNESS ALLPASS (The secret sauce!)
  // Higher coefficient = more inharmonicity
  input = stiffness_allpass(input, &dl->allpass_z1, dl->allpass_coeff);

  // DAMPING LOWPASS
  input = damping_lpf(input, &dl->lpf_z1, damping_cutoff);

  // Write to delay line
  dl->buffer[dl->write_pos] = input;
  dl->write_pos = (dl->write_pos + 1) % MAX_DELAY_LENGTH;

  return input;
}

// COMB FILTER (M1 DAC character @ 2-3kHz)
inline float comb_filter(Voice *v, float input) {
  // Comb filter at ~2500Hz
  uint32_t comb_delay = (uint32_t)(48000.f / 2500.f); // ~19 samples

  uint32_t read_pos = (v->comb_write + 256 - comb_delay) % 256;
  float delayed = v->comb_buffer[read_pos];

  v->comb_buffer[v->comb_write] = input;
  v->comb_write = (v->comb_write + 1) % 256;

  // Mix dry + delayed (creates resonance peak)
  float mix = 0.3f * s_body_resonance;
  return input + delayed * mix;
}

// PEAKING EQ (Body resonance)
inline float peaking_eq(Voice *v, float input, float freq, float q,
                        float gain) {
  // 2nd-order peaking filter
  float w0 = 2.f * PI * freq / 48000.f;
  float phase_norm = w0 / (2.f * PI);
  // Normalize phase to [0,1] for osc_sinf/osc_cosf
  phase_norm = phase_norm - (int32_t)phase_norm;
  if (phase_norm < 0.f)
    phase_norm += 1.f;
  if (phase_norm >= 1.f)
    phase_norm -= 1.f;
  float alpha = osc_sinf(phase_norm) / (2.f * q);
  float A = fastpow2f(gain / 2.f);

  float b0 = 1.f + alpha * A;
  float cos_phase = osc_cosf(phase_norm);
  float b1 = -2.f * cos_phase;
  float b2 = 1.f - alpha * A;
  float a0 = 1.f + alpha / A;
  float a1 = -2.f * cos_phase;
  float a2 = 1.f - alpha / A;

  // Normalize
  b0 /= a0;
  b1 /= a0;
  b2 /= a0;
  a1 /= a0;
  a2 /= a0;

  // Process
  float output = b0 * input + b1 * v->peak_z1 + b2 * v->peak_z2 -
                 a1 * v->peak_z1 - a2 * v->peak_z2;

  v->peak_z2 = v->peak_z1;
  v->peak_z1 = input;

  return output;
}

// CHORUS EFFECT
inline float chorus_process(float x, int channel) {
  float *buffer = (channel == 0) ? s_chorus_buffer_l : s_chorus_buffer_r;

  buffer[s_chorus_write] = x;

  s_chorus_lfo_phase += 0.6f / 48000.f;
  if (s_chorus_lfo_phase >= 1.f)
    s_chorus_lfo_phase -= 1.f;

  float lfo = sine_lookup(s_chorus_lfo_phase);
  float delay_samps =
      600.f + lfo * 300.f * s_chorus_depth + (float)channel * 80.f;

  uint32_t delay_int = (uint32_t)delay_samps;
  uint32_t read_pos =
      (s_chorus_write + CHORUS_BUFFER_SIZE - delay_int) % CHORUS_BUFFER_SIZE;

  float chorus_mix = s_chorus_depth * 0.4f;
  return x * (1.f - chorus_mix) + buffer[read_pos] * chorus_mix;
}

// RELEASE ENVELOPE
inline float process_release(Voice *v) {
  if (v->release_stage == 0)
    return 1.f;

  float t_sec = (float)v->release_counter / 48000.f;
  float release = 0.05f + s_release_time * 1.95f;

  if (t_sec < release) {
    v->release_env = 1.f - (t_sec / release);
  } else {
    v->release_env = 0.f;
    v->active = false;
  }

  v->release_counter++;
  return v->release_env;
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
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

  s_context = static_cast<const unit_runtime_osc_context_t *>(
      desc->hooks.runtime_context);

  init_sine_table();

  for (int v = 0; v < MAX_VOICES; v++) {
    Voice *voice = &s_voices[v];

    voice->exciter_phase_carrier = 0.f;
    voice->exciter_phase_mod = 0.f;
    voice->exciter_env = 0.f;
    voice->exciter_counter = 0;
    voice->exciter_active = false;

    for (int d = 0; d < 2; d++) {
      DelayLine *dl = &voice->delay_line[d];
      for (int i = 0; i < MAX_DELAY_LENGTH; i++) {
        dl->buffer[i] = 0.f;
      }
      dl->write_pos = 0;
      dl->length = 100;
      dl->allpass_z1 = 0.f;
      dl->allpass_coeff = 0.5f;
      dl->lpf_z1 = 0.f;
      dl->feedback = 0.99f;
    }

    for (int i = 0; i < 256; i++) {
      voice->comb_buffer[i] = 0.f;
    }
    voice->comb_write = 0;

    voice->peak_z1 = voice->peak_z2 = 0.f;

    voice->release_env = 1.f;
    voice->release_stage = 0;
    voice->release_counter = 0;

    voice->active = false;
  }

  for (int i = 0; i < CHORUS_BUFFER_SIZE; i++) {
    s_chorus_buffer_l[i] = 0.f;
    s_chorus_buffer_r[i] = 0.f;
  }
  s_chorus_write = 0;
  s_chorus_lfo_phase = 0.f;

  s_hardness = 0.75f;
  s_decay_time = 0.6f;
  s_stiffness = 0.8f;
  s_detune_amount = 0.3f;
  s_brightness = 0.7f;
  s_body_resonance = 0.5f;
  s_chorus_depth = 0.25f;
  s_release_time = 0.4f;
  s_preset = 0;
  s_velocity_sens = 0.5f;

  s_sample_counter = 0;

  return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
  for (int v = 0; v < MAX_VOICES; v++) {
    s_voices[v].exciter_phase_carrier = 0.f;
    s_voices[v].exciter_phase_mod = 0.f;
  }
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
  uint8_t base_note = (s_context->pitch >> 8) & 0xFF;
  uint8_t mod = s_context->pitch & 0xFF;

  for (uint32_t f = 0; f < frames; f++) {
    float sig = 0.f;
    int active_count = 0;

    for (int v = 0; v < MAX_VOICES; v++) {
      Voice *voice = &s_voices[v];
      if (!voice->active)
        continue;

      float w0 = osc_w0f_for_note(voice->note, mod);

      // Calculate delay line length (string length)
      // Fixed: prevent division by zero and overflow
      uint32_t base_length;
      if (w0 < 0.0001f) {
        base_length = MAX_DELAY_LENGTH - 10; // Safety for very low notes
      } else {
        base_length = (uint32_t)(1.f / w0);
        if (base_length < 10)
          base_length = 10; // Minimum length
        if (base_length > MAX_DELAY_LENGTH - 10)
          base_length = MAX_DELAY_LENGTH - 10;
      }

      // 2-OP FM EXCITER (hammer strike)
      float exciter = fm_exciter(voice, s_hardness);

      // Update exciter phases
      if (voice->exciter_active) {
        voice->exciter_phase_carrier += w0;
        voice->exciter_phase_carrier -= (uint32_t)voice->exciter_phase_carrier;
        voice->exciter_phase_mod += w0;
        voice->exciter_phase_mod -= (uint32_t)voice->exciter_phase_mod;
      }

      // DETUNE processing (always use 2 delay lines for stereo/detune)
      float mixed = 0.f;
      int num_strings = 2; // Fixed: always use 2 delay lines

      for (int d = 0; d < num_strings; d++) {
        DelayLine *dl = &voice->delay_line[d];

        // Detune each string slightly
        float detune_factor = 1.f;
        if (d > 0) {
          float detune_cents = ((float)d - 0.5f) * s_detune_amount * 20.f;
          detune_factor = fastpow2f(detune_cents / 1200.f);
        }

        dl->length = (uint32_t)((float)base_length * detune_factor);
        if (dl->length < 10)
          dl->length = 10;
        if (dl->length > MAX_DELAY_LENGTH - 1)
          dl->length = MAX_DELAY_LENGTH - 1;

        // Set stiffness allpass coefficient
        // Higher stiffness = more inharmonicity
        // Fixed: use positive coefficients (0.1 to 0.9) for stability
        dl->allpass_coeff =
            0.1f + s_stiffness * 0.8f; // 0.1 to 0.9 (was -0.9 to -0.1)

        // Set feedback (decay time)
        // Fixed: reduced max feedback to prevent instability
        dl->feedback =
            0.90f + s_decay_time * 0.09f; // 0.90-0.99 (was 0.95-0.9999)

        // Damping cutoff (brightness) - FIXED: now in Hz, not normalized!
        // Range: 200 Hz (dark) to 8000 Hz (bright)
        float damping_cutoff = 200.f + s_brightness * 7800.f;

        // KARPLUS-STRONG with STIFFNESS
        float string_out =
            karplus_strong_process(dl, exciter, s_stiffness, damping_cutoff);

        mixed += string_out;
      }

      mixed /= (float)num_strings;

      // COMB FILTER (M1 DAC character)
      mixed = comb_filter(voice, mixed);

      // PEAKING EQ (Body resonance)
      float peak_freq = 800.f + s_body_resonance * 1200.f;
      float peak_gain = s_body_resonance * 0.5f;
      mixed = peaking_eq(voice, mixed, peak_freq, 2.f, peak_gain);

      // RELEASE ENVELOPE
      float release = process_release(voice);
      mixed *= release;

      if (release < 0.001f && voice->release_stage > 0) {
        voice->active = false;
        continue;
      }

      sig += mixed;
      active_count++;
    }

    if (active_count > 0) {
      sig /= (float)active_count;
    }

    // CHORUS
    sig = chorus_process(sig, 0);

    // Gentle saturation
    sig = fast_tanh(sig * 1.2f);

    out[f] = clipminmaxf(-1.f, sig * 3.5f, 1.f); // Volume boost!

    s_chorus_write = (s_chorus_write + 1) % CHORUS_BUFFER_SIZE;
    s_sample_counter++;
  }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
  value = clipminmaxi32(unit_header.params[id].min, value,
                        unit_header.params[id].max);
  const float valf = param_val_to_f32(value);

  switch (id) {
  case 0:
    s_hardness = valf;
    break;
  case 1:
    s_decay_time = valf;
    break;
  case 2:
    s_stiffness = valf;
    break;
  case 3:
    s_detune_amount = valf;
    break;
  case 4:
    s_brightness = valf;
    break;
  case 5:
    s_body_resonance = valf;
    break;
  case 6:
    s_chorus_depth = valf;
    break;
  case 7:
    s_release_time = valf;
    break;
  case 8:
    s_preset = value;
    s_hardness = s_presets[value].hardness;
    s_decay_time = s_presets[value].decay;
    s_stiffness = s_presets[value].stiffness;
    s_detune_amount = s_presets[value].detune;
    s_brightness = s_presets[value].brightness;
    s_body_resonance = s_presets[value].body;
    s_chorus_depth = s_presets[value].chorus;
    s_release_time = s_presets[value].release;
    break;
  case 9:
    s_velocity_sens = valf;
    break;
  default:
    break;
  }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
  switch (id) {
  case 0:
    return (int32_t)(s_hardness * 1023.f);
  case 1:
    return (int32_t)(s_decay_time * 1023.f);
  case 2:
    return (int32_t)(s_stiffness * 1023.f);
  case 3:
    return (int32_t)(s_detune_amount * 1023.f);
  case 4:
    return (int32_t)(s_brightness * 1023.f);
  case 5:
    return (int32_t)(s_body_resonance * 1023.f);
  case 6:
    return (int32_t)(s_chorus_depth * 1023.f);
  case 7:
    return (int32_t)(s_release_time * 1023.f);
  case 8:
    return s_preset;
  case 9:
    return (int32_t)(s_velocity_sens * 1023.f);
  default:
    return 0;
  }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id,
                                                     int32_t value) {
  if (id == 8) {
    static const char *preset_names[] = {"M1PIANO", "HOUSE", "RHODES", "TRANCE",
                                         "BELL",    "WURLI", "DETUNE", "LOFI"};
    return preset_names[value];
  }
  return "";
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velo) {
  int free_voice = -1;
  for (int v = 0; v < MAX_VOICES; v++) {
    if (!s_voices[v].active) {
      free_voice = v;
      break;
    }
  }

  if (free_voice == -1)
    free_voice = 0;

  Voice *voice = &s_voices[free_voice];
  voice->note = note;
  voice->velocity = velo;
  voice->active = true;

  // Trigger exciter
  voice->exciter_phase_carrier = 0.f;
  voice->exciter_phase_mod = 0.f;
  voice->exciter_env = 1.f;
  voice->exciter_counter = 0;
  voice->exciter_active = true;

  // CRITICAL: PRE-FILL delay lines with STRONG exciter burst!
  float w0_init = osc_w0f_for_note(note, 0);

  // Calculate delay line length FIRST (before pre-fill)
  uint32_t base_length_init;
  if (w0_init < 0.0001f) {
    base_length_init = MAX_DELAY_LENGTH - 10;
  } else {
    base_length_init = (uint32_t)(1.f / w0_init);
    if (base_length_init < 10)
      base_length_init = 10;
    if (base_length_init > MAX_DELAY_LENGTH - 10)
      base_length_init = MAX_DELAY_LENGTH - 10;
  }

  // Set delay line lengths BEFORE pre-fill
  for (int d = 0; d < 2; d++) {
    DelayLine *dl = &voice->delay_line[d];

    // FIX: Clear buffer to prevent "whistle"/garbage feedback from previous
    // notes!
    for (int i = 0; i < MAX_DELAY_LENGTH; i++) {
      dl->buffer[i] = 0.f;
    }

    float detune_factor = 1.f;
    if (d > 0) {
      float detune_cents = ((float)d - 0.5f) * s_detune_amount * 20.f;
      detune_factor = fastpow2f(detune_cents / 1200.f);
    }
    dl->length = (uint32_t)((float)base_length_init * detune_factor);
    if (dl->length < 10)
      dl->length = 10;
    if (dl->length > MAX_DELAY_LENGTH - 1)
      dl->length = MAX_DELAY_LENGTH - 1;
  }

  // Generate 500 samples of exciter (longer burst!)
  for (int burst = 0; burst < 500; burst++) {
    // Generate exciter sample
    float fm_ratio = 4.2f;
    float mod_index = 10.f + s_hardness * 6.f;

    float mod_phase_norm = voice->exciter_phase_carrier * fm_ratio;
    mod_phase_norm -= (uint32_t)mod_phase_norm;
    if (mod_phase_norm < 0.f)
      mod_phase_norm += 1.f;

    float modulator = sine_lookup(mod_phase_norm);

    float carrier_phase_mod =
        voice->exciter_phase_carrier + mod_index * modulator;
    carrier_phase_mod -= (uint32_t)carrier_phase_mod;
    if (carrier_phase_mod < 0.f)
      carrier_phase_mod += 1.f;

    float carrier = sine_lookup(carrier_phase_mod);

    float exciter_burst = carrier * 3.0f; // STRONG burst!

    // Fill ALL delay lines
    for (int d = 0; d < 2; d++) {
      DelayLine *dl = &voice->delay_line[d];

      // Write to multiple positions (spread the energy)
      for (int spread = 0; spread < 5; spread++) {
        uint32_t write_idx = (dl->write_pos + spread) % MAX_DELAY_LENGTH;
        dl->buffer[write_idx] += exciter_burst * 0.2f;
      }

      dl->write_pos = (dl->write_pos + 1) % MAX_DELAY_LENGTH;
    }

    voice->exciter_phase_carrier += w0_init;
    voice->exciter_phase_carrier -= (uint32_t)voice->exciter_phase_carrier;
    voice->exciter_phase_mod += w0_init;
    voice->exciter_phase_mod -= (uint32_t)voice->exciter_phase_mod;
  }

  // Reset exciter for runtime
  voice->exciter_counter = 0;
  voice->exciter_active = true;
  voice->exciter_phase_carrier = 0.f;
  voice->exciter_phase_mod = 0.f;

  // Reset delay lines filter state and set coefficients
  for (int d = 0; d < 2; d++) {
    DelayLine *dl = &voice->delay_line[d];
    dl->allpass_z1 = 0.f;
    dl->lpf_z1 = 0.f;
    dl->allpass_coeff = 0.1f + s_stiffness * 0.8f; // Set stiffness coefficient
    dl->feedback = 0.90f + s_decay_time * 0.09f;   // Set feedback
  }

  // Reset release
  voice->release_stage = 0;
  voice->release_counter = 0;
  voice->release_env = 1.f;
}

__unit_callback void unit_note_off(uint8_t note) {
  for (int v = 0; v < MAX_VOICES; v++) {
    if (s_voices[v].note == note && s_voices[v].active) {
      s_voices[v].release_stage = 1;
      s_voices[v].release_counter = 0;
    }
  }
}

__unit_callback void unit_all_note_off() {
  for (int v = 0; v < MAX_VOICES; v++) {
    s_voices[v].active = false;
  }
}

__unit_callback void unit_set_tempo(uint32_t tempo) {}
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {}

__unit_callback void unit_pitch_bend(uint16_t bend) {}

__unit_callback void unit_channel_pressure(uint8_t press) {}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t press) {}
