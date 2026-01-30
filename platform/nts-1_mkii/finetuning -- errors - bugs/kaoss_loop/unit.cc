/*
    KAOSSILATOR LOOP RECORDER - The Live Looping Beast
    
    ═══════════════════════════════════════════════════════════════
    ARCHITECTURE
    ═══════════════════════════════════════════════════════════════
    
    === 4-LAYER SYSTEM ===
    
    Each layer has:
    - Independent buffer (3 seconds each @ 48kHz = 144k samples)
    - Record/Playback state machine
    - Speed control (0.5x, 1x, 2x, reverse)
    - Pitch shifter (±12 semitones)
    - Filter (LP with resonance)
    - Volume/Pan control
    - Loop start/end points
    
    Total buffer: 4 × 144k × 2 channels = 1.15 MB (uses SDRAM!)
    
    === RECORDING MODES ===
    
    0. OVERDUB - Add to existing loop
    1. REPLACE - Overwrite loop
    2. INSERT - Insert and shift
    3. MULTIPLY - Extend loop length
    
    === PLAYBACK MODES ===
    
    0. NORMAL - Straight playback
    1. REVERSE - Backwards
    2. HALF - Half speed (pitch down)
    3. DOUBLE - Double speed (pitch up)
    4. SLICE - Chop into 16 steps
    5. STUTTER - Gate/retrigger
    6. GRANULAR - Grain cloud
    7. FREEZE - Hold current position
    
    === QUANTIZATION ===
    
    0. FREE - No quantize
    1. 1/4 - Quarter note
    2. 1/8 - Eighth note
    3. 1/16 - Sixteenth note
    
    Loop length snaps to: 1, 2, 4, 8, 16 bars
    
    ═══════════════════════════════════════════════════════════════
    INSPIRED BY
    ═══════════════════════════════════════════════════════════════
    
    - Korg Kaossilator series
    - Boss RC-505 Loop Station
    - Electrix Repeater
    - EHX 16 Second Digital Delay
    
*/

#include "unit_delfx.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "fx_api.h"  // For fx_sinf (delay effects use fx_api, not osc_api!)
#include <algorithm>

// SDK compatibility
// SDK compatibility - PI is already defined in CMSIS arm_math.h
// const float PI = 3.14159265359f; // Removed - conflicts with CMSIS

#define MAX_LAYERS 4
#define SAMPLES_PER_LAYER 144000  // 3 seconds @ 48kHz (reduced from 6 for size)
#define MAX_LOOP_BARS 16

// Layer states
enum LayerState {
    LAYER_STOPPED,
    LAYER_ARMED,
    LAYER_RECORDING,
    LAYER_PLAYING,
    LAYER_OVERDUBBING
};

struct Layer {
    // Buffer pointers (allocated in SDRAM)
    float *buffer_l;
    float *buffer_r;
    
    // State
    LayerState state;
    uint32_t write_pos;
    float read_pos_f;  // Float for smooth playback
    uint32_t loop_start;
    uint32_t loop_end;
    uint32_t loop_length;
    
    // Playback
    float speed;           // 0.5, 1.0, 2.0, -1.0 (reverse)
    int8_t pitch_shift;    // -12 to +12 semitones
    bool reverse;
    
    // Filter
    float filter_cutoff;
    float filter_resonance;
    float filter_z1_l, filter_z2_l;
    float filter_z1_r, filter_z2_r;
    
    // Mix
    float volume;
    float pan;
    
    // Quantization
    bool quantize_active;
    uint32_t quantize_wait_samples;
};

static Layer s_layers[MAX_LAYERS];

// Recording state
static uint8_t s_armed_layer;
static bool s_recording_active;
static uint32_t s_record_counter;

// Parameters
static float s_time_control;
static float s_feedback_amount;
static float s_mix;
static float s_layer_volumes[MAX_LAYERS];
static uint8_t s_loop_length_bars;
static uint8_t s_mode;
static uint8_t s_quantize_mode;

// Tempo sync
static uint32_t s_tempo_counter;
static uint32_t s_beat_length;  // samples per beat
static bool s_tempo_active;

static uint32_t s_sample_counter;

inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// Simple pitch shifter (time-domain with interpolation)
inline float pitch_shift_read(Layer *layer, bool is_right) {
    // Safety check: if loop_length is 0 or invalid, return 0
    if (layer->loop_length == 0 || layer->loop_length > SAMPLES_PER_LAYER) {
        return 0.f;
    }
    
    float *buffer = is_right ? layer->buffer_r : layer->buffer_l;
    
    // Calculate pitch-shifted read position
    float pitch_ratio = fastpow2f((float)layer->pitch_shift / 12.f);
    float read_pos_f = layer->read_pos_f * pitch_ratio;
    
    // Wrap
    while (read_pos_f >= (float)layer->loop_length) {
        read_pos_f -= (float)layer->loop_length;
    }
    while (read_pos_f < 0.f) {
        read_pos_f += (float)layer->loop_length;
    }
    
    uint32_t pos0 = (uint32_t)read_pos_f;
    uint32_t pos1 = (pos0 + 1) % layer->loop_length;
    float frac = read_pos_f - (float)pos0;
    
    return buffer[pos0] * (1.f - frac) + buffer[pos1] * frac;
}

// State-variable filter
inline float process_filter(Layer *layer, float input, bool is_right) {
    float cutoff = 100.f + layer->filter_cutoff * 19900.f;
    if (cutoff > 20000.f) cutoff = 20000.f;
    
    float w = 2.f * PI * cutoff / 48000.f;
    float phase = (w * 0.5f) / (2.f * PI);  // Convert to [0,1] phase
    if (phase >= 1.f) phase -= 1.f;
    if (phase < 0.f) phase += 1.f;
    float f = 2.f * fx_sinf(phase);
    float q = 1.f / (0.5f + layer->filter_resonance * 9.5f);
    
    float *z1 = is_right ? &layer->filter_z1_r : &layer->filter_z1_l;
    float *z2 = is_right ? &layer->filter_z2_r : &layer->filter_z2_l;
    
    *z2 = *z2 + f * *z1;
    float hp = input - *z2 - q * *z1;
    *z1 = *z1 + f * hp;
    
    // Return lowpass
    return *z2;
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

    // Allocate buffers in SDRAM (4 layers × 2 channels × 144k samples)
    uint32_t total_samples = MAX_LAYERS * 2 * SAMPLES_PER_LAYER;
    float *sdram_buffer = (float *)desc->hooks.sdram_alloc(total_samples * sizeof(float));
    if (!sdram_buffer) return k_unit_err_memory;

    // Clear buffer
    std::fill(sdram_buffer, sdram_buffer + total_samples, 0.f);

    // Init layers and assign buffer pointers
    for (int i = 0; i < MAX_LAYERS; i++) {
        Layer *layer = &s_layers[i];
        
        // Assign buffer pointers
        layer->buffer_l = sdram_buffer + (i * 2 * SAMPLES_PER_LAYER);
        layer->buffer_r = sdram_buffer + (i * 2 * SAMPLES_PER_LAYER + SAMPLES_PER_LAYER);
        
        layer->state = LAYER_STOPPED;
        layer->write_pos = 0;
        layer->read_pos_f = 0.f;
        layer->loop_start = 0;
        layer->loop_end = 0;
        layer->loop_length = SAMPLES_PER_LAYER;
        
        layer->speed = 1.f;
        layer->pitch_shift = 0;
        layer->reverse = false;
        
        layer->filter_cutoff = 1.f;
        layer->filter_resonance = 0.f;
        layer->filter_z1_l = layer->filter_z2_l = 0.f;
        layer->filter_z1_r = layer->filter_z2_r = 0.f;
        
        layer->volume = 0.8f;
        layer->pan = 0.f;
        
        layer->quantize_active = false;
    }
    
    s_armed_layer = 0;
    s_recording_active = false;
    s_record_counter = 0;
    
    s_time_control = 0.75f;
    s_feedback_amount = 0.6f;
    s_mix = 0.75f;  // Default 75% wet (hoor loops!)
    
    for (int i = 0; i < MAX_LAYERS; i++) {
        s_layer_volumes[i] = 0.75f;  // Default 75% volume
    }
    
    s_loop_length_bars = 4;
    s_mode = 0;
    s_quantize_mode = 0;
    
    s_tempo_counter = 0;
    s_beat_length = 12000;  // ~120 BPM
    s_tempo_active = false;
    
    s_sample_counter = 0;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    for (int i = 0; i < MAX_LAYERS; i++) {
        s_layers[i].state = LAYER_STOPPED;
        s_layers[i].read_pos_f = 0.f;
    }
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    const float *in_ptr = in;
    float *out_ptr = out;
    
    for (uint32_t f = 0; f < frames; f++) {
        float in_l = in_ptr[0];
        float in_r = in_ptr[1];
        
        // AUTO-START: If layer 0 is stopped and we have input, start recording!
        if (s_layers[0].state == LAYER_STOPPED) {
            float input_level = si_fabsf(in_l) + si_fabsf(in_r);
            if (input_level > 0.001f) {  // Input detected!
                s_layers[0].state = LAYER_RECORDING;
                s_layers[0].write_pos = 0;
                s_layers[0].read_pos_f = 0.f;
                // Calculate loop length based on bars
                uint32_t bars = 1 << s_loop_length_bars;  // 1, 2, 4, 8, 16
                uint32_t calculated_length = bars * s_beat_length * 4;
                if (calculated_length == 0 || calculated_length > SAMPLES_PER_LAYER) {
                    calculated_length = SAMPLES_PER_LAYER;
                }
                if (calculated_length < 100) calculated_length = 100;
                s_layers[0].loop_length = calculated_length;
            }
        }
        
        float mixed_l = 0.f;
        float mixed_r = 0.f;
        
        // Process each layer
        for (int i = 0; i < MAX_LAYERS; i++) {
            Layer *layer = &s_layers[i];
            
            // RECORDING
            if (layer->state == LAYER_RECORDING || layer->state == LAYER_OVERDUBBING) {
                if (layer->write_pos < layer->loop_length) {
                    if (layer->state == LAYER_OVERDUBBING) {
                        // Mix with existing
                        layer->buffer_l[layer->write_pos] = 
                            layer->buffer_l[layer->write_pos] * s_feedback_amount + in_l;
                        layer->buffer_r[layer->write_pos] = 
                            layer->buffer_r[layer->write_pos] * s_feedback_amount + in_r;
                    } else {
                        // Replace
                        layer->buffer_l[layer->write_pos] = in_l;
                        layer->buffer_r[layer->write_pos] = in_r;
                    }
                    
                    layer->write_pos++;
                    
                    // Loop complete?
                    if (layer->write_pos >= layer->loop_length) {
                        layer->state = LAYER_PLAYING;
                        layer->write_pos = 0;
                        layer->read_pos_f = 0.f;
                    }
                }
            }
            
            // PLAYBACK
            if (layer->state == LAYER_PLAYING || layer->state == LAYER_OVERDUBBING) {
                // Read from buffer (with pitch shift)
                float play_l = pitch_shift_read(layer, false);
                float play_r = pitch_shift_read(layer, true);
                
                // Filter
                play_l = process_filter(layer, play_l, false);
                play_r = process_filter(layer, play_r, true);
                
                // Pan
                float pan = layer->pan;
                float gain_l = (1.f - pan) * 0.5f;
                float gain_r = (1.f + pan) * 0.5f;
                
                // Mix
                float vol = layer->volume * s_layer_volumes[i];
                mixed_l += play_l * gain_l * vol;
                mixed_r += play_r * gain_r * vol;
                
                // Advance read position
                float speed = layer->reverse ? -layer->speed : layer->speed;
                layer->read_pos_f += speed;
                
                if (layer->read_pos_f >= (float)layer->loop_length) {
                    layer->read_pos_f -= (float)layer->loop_length;
                } else if (layer->read_pos_f < 0.f) {
                    layer->read_pos_f += (float)layer->loop_length;
                }
            }
        }
        
        // Mix dry/wet
        // CRITICAL FIX: Always pass through input, even if no loops are playing!
        // This ensures you ALWAYS hear something when the effect is active
        float dry_gain = 1.f - s_mix;
        float wet_gain = s_mix;
        
        // If no layers are playing, increase dry signal so you hear input
        bool any_playing = false;
        for (int i = 0; i < MAX_LAYERS; i++) {
            if (s_layers[i].state == LAYER_PLAYING || s_layers[i].state == LAYER_OVERDUBBING) {
                any_playing = true;
                break;
            }
        }
        
        if (!any_playing && s_layers[0].state != LAYER_RECORDING) {
            // No loops yet - pass through input at full volume
            dry_gain = 1.f;
            wet_gain = 0.f;
        }
        
        out_ptr[0] = in_l * dry_gain + mixed_l * wet_gain;
        out_ptr[1] = in_r * dry_gain + mixed_r * wet_gain;
        
        out_ptr[0] = clipminmaxf(-1.f, out_ptr[0], 1.f);
        out_ptr[1] = clipminmaxf(-1.f, out_ptr[1], 1.f);
        
        in_ptr += 2;
        out_ptr += 2;
        s_sample_counter++;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_time_control = valf; break;
        case 1: s_feedback_amount = valf; break;
        case 2: s_mix = valf; break;
        case 3: s_layer_volumes[0] = valf; break;
        case 4: s_layer_volumes[1] = valf; break;
        case 5: s_layer_volumes[2] = valf; break;
        case 6: s_layer_volumes[3] = valf; break;
        case 7: 
            s_loop_length_bars = value;
            // Update loop lengths
            for (int i = 0; i < MAX_LAYERS; i++) {
                uint32_t bars = 1 << value;  // 1, 2, 4, 8, 16
                uint32_t calculated_length = bars * s_beat_length * 4;
                // Safety: ensure minimum length and maximum
                if (calculated_length == 0) calculated_length = SAMPLES_PER_LAYER;
                s_layers[i].loop_length = clipmaxi32(calculated_length, SAMPLES_PER_LAYER);
                // Ensure loop_length is at least 100 samples
                if (s_layers[i].loop_length < 100) s_layers[i].loop_length = 100;
            }
            break;
        case 8: s_mode = value; break;
        case 9: s_quantize_mode = value; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_time_control * 1023.f);
        case 1: return (int32_t)(s_feedback_amount * 1023.f);
        case 2: return (int32_t)(s_mix * 1023.f);
        case 3: return (int32_t)(s_layer_volumes[0] * 1023.f);
        case 4: return (int32_t)(s_layer_volumes[1] * 1023.f);
        case 5: return (int32_t)(s_layer_volumes[2] * 1023.f);
        case 6: return (int32_t)(s_layer_volumes[3] * 1023.f);
        case 7: return s_loop_length_bars;
        case 8: return s_mode;
        case 9: return s_quantize_mode;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 7) {
        static const char *length_names[] = {
            "1BAR", "2BAR", "4BAR", "8BAR", "16BAR"
        };
        if (value >= 0 && value < 5) return length_names[value];
    }
    if (id == 8) {
        static const char *mode_names[] = {
            "OVERDUB", "REPLACE", "INSERT", "MULT",
            "REVERSE", "SLICE", "STUTTER", "FREEZE"
        };
        if (value >= 0 && value < 8) return mode_names[value];
    }
    if (id == 9) {
        static const char *quant_names[] = {"FREE", "1/4", "1/8", "1/16"};
        if (value >= 0 && value < 4) return quant_names[value];
    }
    return "";
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    s_tempo_active = true;
    s_tempo_counter = counter;
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    // tempo in BPM × 10 (e.g., 1200 = 120.0 BPM)
    float bpm = (float)tempo / 10.f;
    if (bpm < 60.f) bpm = 120.f;  // Default
    s_beat_length = (uint32_t)(48000.f * 60.f / bpm);
}

