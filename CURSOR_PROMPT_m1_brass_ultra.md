# 🚨 KRITIEK: M1 BRASS ULTRA - GEEN OUTPUT GELUID

## 📋 PROBLEEM
De `m1_brass_ultra` oscillator produceert **GEEN geluid** na de laatste fix. Alle parameters werken, maar er is **complete stilte** bij alle settings.

## 🎯 ANALYSE INSTRUCTIES VOOR CURSOR

**Bestand te analyseren:** `platform/nts-1_mkii/m1_brass_ultra/unit.cc`

### **STAP 1: VERIFY SDK FUNCTIONS - KRITIEK!**

Check ALLE gebruik van `fx_pow2f()` - dit is een **EFFECT functie**, niet een OSCILLATOR functie!

**Zoek alle occurrences:**
```bash
grep -n "fx_pow2f\|osc_pow2f\|fastpow2f" unit.cc
```

**VERVANG ALLE `fx_pow2f()` met `osc_pow2f()`:**
```cpp
// ❌ FOUT (effect functie in oscillator!):
float w0 = base_w0 * fx_pow2f(detune_cents / 1200.f);
v->transient_env = fx_pow2f(-t_sec / patch->transient_decay * 5.f);
float env = fx_pow2f(-t_sec / patch->pitch_env_time * 5.f);
v->amp_env = patch->sustain + (1.f - patch->sustain) * fx_pow2f(-t_sec / patch->decay * 5.f);
v->amp_env = patch->sustain * fx_pow2f(-t_sec / release * 5.f);
s_transient_table[i] = osc_sinf(phase_sin) * fx_pow2f(-phase * 3.f);

// ✅ CORRECT (oscillator functie!):
float w0 = base_w0 * osc_pow2f(detune_cents / 1200.f);
v->transient_env = osc_pow2f(-t_sec / patch->transient_decay * 5.f);
float env = osc_pow2f(-t_sec / patch->pitch_env_time * 5.f);
v->amp_env = patch->sustain + (1.f - patch->sustain) * osc_pow2f(-t_sec / patch->decay * 5.f);
v->amp_env = patch->sustain * osc_pow2f(-t_sec / release * 5.f);
s_transient_table[i] = osc_sinf(phase_sin) * osc_pow2f(-phase * 3.f);
```

**VERIFY INCLUDES:**
```cpp
// ✅ MUST have:
#include "osc_api.h"  // For osc_pow2f()
// ❌ REMOVE if present:
// #include "fx_api.h"  // This is for EFFECTS, not oscillators!
```

---

### **STAP 2: CHECK UNIT_INIT DEFAULTS**

Zoek `unit_init()` functie (rond regel 727) en verify defaults:

```cpp
// ✅ CORRECT defaults (check deze!):
s_brightness = 0.6f;        // 60%
s_resonance = 0.75f;         // 75%
s_detune_amount = 0.5f;      // 50%
s_ensemble_amount = 0.4f;    // 40% - NIET 0.0f!
s_vibrato_amount = 0.4f;     // 40%
s_breath_amount = 0.25f;     // 25% - NIET 0.0f!
s_attack_mod = 0.65f;        // 65%
s_release_mod = 0.8f;        // 80%
s_patch_select = 0;          // Patch 0
s_voice_count = 2;           // 5 voices active (not 0!)
```

**KRITIEK:** Als `s_breath_amount = 0.0f` of `s_ensemble_amount = 0.0f` → **GEEN GELUID!**

---

### **STAP 3: CHECK UNIT_RENDER OUTPUT**

Zoek `unit_render()` functie (rond regel 822) en verify output:

```cpp
// ✅ CORRECT output (rond regel 904-905):
float mono = (sig_l + sig_r) * 0.5f;
// ... DC blocker ...
out[f] = clipminmaxf(-1.f, mono * 2.5f, 1.f);  // ✅ 2.5× gain
```

**VERIFY:**
- ✅ Output gain = **2.5×** (niet 0.8× of 1.0×!)
- ✅ `mono` wordt berekend uit `sig_l` en `sig_r`
- ✅ `sig_l` en `sig_r` worden opgebouwd in de voice loop

---

### **STAP 4: CHECK VOICE ACTIVATION**

In `unit_render()`, verify dat voices correct worden geactiveerd:

```cpp
for (int v = 0; v < MAX_VOICES; v++) {
    Voice *voice = &s_voices[v];
    if (!voice->active) continue;  // ✅ Skip inactive voices
    
    // ... processing ...
    
    sig_l += ens_l;
    sig_r += ens_r;
    active_count++;
}

// ✅ VERIFY: active_count > 0 check
if (active_count > 0) {
    sig_l /= (float)active_count;
    sig_r /= (float)active_count;
}
```

**PROBLEEM:** Als `active_count == 0` → `sig_l` en `sig_r` blijven 0 → **GEEN GELUID!**

---

### **STAP 5: CHECK ENVELOPE & BREATH**

Verify dat envelope en breath controller werken:

```cpp
// In unit_render(), rond regel 866-876:
float env = update_envelope(voice, patch);
float vel_scale = (float)voice->velocity / 127.f;
vel_scale = 0.5f + vel_scale * 0.5f;

voice->breath_level += (s_breath_amount - voice->breath_level) * 0.001f;

ens_l *= env * vel_scale * voice->breath_level;
ens_r *= env * vel_scale * voice->breath_level;
```

**KRITIEK PROBLEMEN:**
1. Als `env == 0.f` → **GEEN GELUID!**
2. Als `voice->breath_level == 0.f` → **GEEN GELUID!**
3. Als `s_breath_amount == 0.f` → `voice->breath_level` blijft 0 → **GEEN GELUID!**

**FIX:** Add safety checks:
```cpp
// ✅ SAFETY: Ensure breath_level is never 0 if breath_amount > 0
if (s_breath_amount > 0.01f && voice->breath_level < 0.01f) {
    voice->breath_level = s_breath_amount;  // Initialize!
}

// ✅ SAFETY: Ensure envelope is valid
if (env < 0.f) env = 0.f;
if (env > 1.f) env = 1.f;
if (!std::isfinite(env)) env = 0.f;
```

---

### **STAP 6: CHECK ENSEMBLE GENERATION**

Verify `generate_ensemble()` functie (rond regel 440):

```cpp
inline void generate_ensemble(Voice *v, float base_w0, const M1Patch *patch, float *out_l, float *out_r) {
    // ✅ VERIFY: voices_active calculation
    int voices_active = (s_voice_count == 0) ? 1 : (s_voice_count == 1) ? 2 : (s_voice_count == 2) ? 5 : 10;
    
    // ✅ VERIFY: ensemble_amount is used
    float pan = s_ensemble_pan[i] * s_ensemble_amount;
    
    // ✅ VERIFY: output is set (not just initialized to 0)
    *out_l = sum_l;
    *out_r = sum_r;
}
```

**PROBLEEM:** Als `s_ensemble_amount == 0.0f` → `pan = 0` → mogelijk geen output!

---

### **STAP 7: CHECK NaN/Inf PROPAGATION**

Add NaN/Inf detection in `unit_render()`:

```cpp
// ✅ After ensemble generation (rond regel 850):
generate_ensemble(voice, w0, patch, &ens_l, &ens_r);

// ✅ SAFETY: Check for NaN/Inf
if (!std::isfinite(ens_l)) ens_l = 0.f;
if (!std::isfinite(ens_r)) ens_r = 0.f;

// ✅ After formant processing (rond regel 863):
process_formants(voice, patch, &ens_l, &ens_r);

// ✅ SAFETY: Check again
if (!std::isfinite(ens_l)) ens_l = 0.f;
if (!std::isfinite(ens_r)) ens_r = 0.f;

// ✅ Before final output (rond regel 895):
float mono = (sig_l + sig_r) * 0.5f;

// ✅ SAFETY: Final check
if (!std::isfinite(mono)) mono = 0.f;
```

---

### **STAP 8: CHECK UNIT_NOTE_ON**

Verify `unit_note_on()` functie activeert voices correct:

```cpp
__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
    // ✅ VERIFY: Voice is activated
    voice->active = true;
    voice->note = note;
    voice->velocity = velocity;
    
    // ✅ VERIFY: Envelope is reset
    voice->amp_env = 0.f;
    voice->env_stage = 0;
    voice->env_counter = 0;
    
    // ✅ VERIFY: Breath level is initialized
    voice->breath_level = s_breath_amount;  // ✅ CRITICAL!
}
```

**KRITIEK:** Als `voice->breath_level` niet wordt geïnitialiseerd → blijft 0 → **GEEN GELUID!**

---

### **STAP 9: CHECK W0 CALCULATION**

Verify frequency calculation (rond regel 846):

```cpp
// ✅ CORRECT:
float w0 = osc_w0f_for_note(voice->note + (int8_t)pitch_mod, mod);

// ✅ VERIFY: w0 is valid
if (w0 <= 0.f || w0 > 0.48f) {
    w0 = osc_w0f_for_note(voice->note, 0);  // Fallback
}
```

---

### **STAP 10: COMPLETE SAFETY CHECKLIST**

**Cursor moet verifiëren:**

- [ ] **ALL `fx_pow2f()` → `osc_pow2f()`** (7 occurrences!)
- [ ] **Remove `#include "fx_api.h"`** (if present)
- [ ] **Verify `#include "osc_api.h"`** is present
- [ ] **`s_breath_amount` default = 0.25f** (NOT 0.0f!)
- [ ] **`s_ensemble_amount` default = 0.4f** (NOT 0.0f!)
- [ ] **`voice->breath_level` initialized in `unit_note_on()`**
- [ ] **Output gain = 2.5×** (not lower!)
- [ ] **NaN/Inf detection** in `unit_render()`
- [ ] **Envelope safety checks** (0 ≤ env ≤ 1)
- [ ] **w0 validation** (0 < w0 ≤ 0.48)

---

## 🧪 EXPECTED RESULT

**NA fix:**
- ✅ **Compileert clean** (no errors)
- ✅ **Geluid vanaf eerste note**
- ✅ **Envelope werkt correct**
- ✅ **Breath controller werkt**
- ✅ **Ensemble spread werkt**
- ✅ **Volume matcht andere OSCs**

---

## 🎯 ROOT CAUSE HYPOTHESIS

**MEEST WAARSCHIJNLIJK:**
1. **`fx_pow2f()` gebruikt in oscillator** → moet `osc_pow2f()` zijn!
2. **`s_breath_amount = 0.0f`** → `voice->breath_level` blijft 0 → output = 0
3. **`voice->breath_level` niet geïnitialiseerd** in `unit_note_on()`

**DE OPLOSSING:**
- Replace ALL `fx_pow2f()` with `osc_pow2f()`
- Verify breath_amount default > 0
- Initialize `voice->breath_level` in `unit_note_on()`
- Add NaN/Inf safety checks

---

## 📝 VERIFICATION COMMANDS

```bash
# Find all fx_pow2f (should be 0 after fix):
grep -n "fx_pow2f" unit.cc

# Find all osc_pow2f (should be 7 after fix):
grep -n "osc_pow2f" unit.cc

# Find breath_amount default:
grep -n "s_breath_amount.*=" unit.cc | grep "unit_init\|0\."

# Find breath_level initialization:
grep -n "breath_level.*=" unit.cc
```

---

**DIT IS DE COMPLETE ANALYSE!** 🔍✨

**De bug is waarschijnlijk: `fx_pow2f()` in oscillator → moet `osc_pow2f()` zijn!** 🎺🔥

