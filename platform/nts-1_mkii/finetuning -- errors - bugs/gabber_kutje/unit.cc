/*
    GABBER_Kutje - Unit Integration
    SDK glue code
*/

#include "gabber.h"
#include "unit_osc.h"
#include "osc_api.h"
#include "utils/int_math.h"

static Gabber s_gabber_instance;
static int32_t cached_values[UNIT_OSC_MAX_PARAM_COUNT];
static const unit_runtime_osc_context_t *context;

// ========== CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc)
        return k_unit_err_undef;
    
    if (desc->target != unit_header.target)
        return k_unit_err_target;
    
    if (!UNIT_API_IS_COMPAT(desc->api))
        return k_unit_err_api_version;
    
    if (desc->samplerate != s_gabber_instance.getSampleRate())
        return k_unit_err_samplerate;
    
    if (desc->input_channels != 2 || desc->output_channels != 1)
        return k_unit_err_geometry;
    
    // Store context
    context = static_cast<const unit_runtime_osc_context_t *>(desc->hooks.runtime_context);
    
    // Initialize oscillator
    s_gabber_instance.init();
    
    // Initialize cached parameters
    for (uint8_t id = 0; id < UNIT_OSC_MAX_PARAM_COUNT; ++id) {
        cached_values[id] = unit_header.params[id].init;
    }
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {
    // Nothing to cleanup
}

__unit_callback void unit_reset() {
    s_gabber_instance.init();
}

__unit_callback void unit_resume() {}

__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    (void)in;  // Not using audio input
    
    // Get current pitch from context (w0 is normalized frequency)
    const float w0 = osc_w0f_for_note((context->pitch >> 8) & 0xFF, context->pitch & 0xFF);
    const uint8_t note = (context->pitch >> 8) & 0xFF;
    const uint8_t mod = 0;  // Not using mod wheel
    
    // Process audio
    s_gabber_instance.process(w0, note, mod, (q31_t *)out, frames);
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    cached_values[id] = value;
    s_gabber_instance.setParameter(id, value);
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    return cached_values[id];
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    return s_gabber_instance.getParameterStrValue(id, value);
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    (void)tempo;  // Not using tempo (pump is fixed 128 BPM)
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
    s_gabber_instance.noteOn(note, velocity);
}

__unit_callback void unit_note_off(uint8_t note) {
    (void)note;
    s_gabber_instance.noteOff();
}

__unit_callback void unit_all_note_off() {
    s_gabber_instance.noteOff();
}

__unit_callback void unit_pitch_bend(uint16_t bend) {
    (void)bend;
}

__unit_callback void unit_channel_pressure(uint8_t press) {
    (void)press;
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t press) {
    (void)note;
    (void)press;
}

