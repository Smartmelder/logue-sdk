# M1 PIANO - Sample-Based Oscillator

A professional **Korg M1-style piano** oscillator for the **KORG NTS-1 MKII** using the classic M1 method: separate attack sample + looping cycle.

![Status](https://img.shields.io/badge/Status-Stable-green)
![Platform](https://img.shields.io/badge/Platform-NTS--1_MKII-blue)
![Type](https://img.shields.io/badge/Type-Oscillator-orange)
![Size](https://img.shields.io/badge/Size-16KB-brightgreen)

---

## 🎹 Features

✅ **M1-Method** - Attack + Loop samples (like original Korg M1)
✅ **Natural Sound** - Extracted from real piano WAV
✅ **10 M1-Style Parameters** - Complete control over the sound
✅ **Velocity Sensitive** - Responds to MIDI velocity
✅ **Vibrato LFO** - Adjustable depth and speed
✅ **Filter** - Brightness and resonance control
✅ **Full ADSR** - Attack, Decay, Sustain, Release
✅ **Stereo Width** - For wider sound
✅ **Detune** - Fine tuning (-100 to +100 cents)

---

## 🎛️ 10 M1-Style Parameters

| # | Parameter | Function | Range | Default |
|---|-----------|----------|-------|---------|
| 1 | **DECAY** | Decay time after attack | 0.1 - 5.0s | 1.0s |
| 2 | **RELEASE** | Release time after note off | 0.01 - 3.0s | 0.5s |
| 3 | **BRIGHTNESS** | Filter cutoff frequency | 0.1 - 1.0 | 0.8 |
| 4 | **RESONANCE** | Filter resonance/Q | 0 - 100% | 20% |
| 5 | **VIBRATO** | Vibrato depth | 0 - 100% | 0% |
| 6 | **VSPEED** | Vibrato speed | 0.5 - 10 Hz | 5 Hz |
| 7 | **ATTACK** | Attack time | 0.001 - 1.0s | 0.001s |
| 8 | **SUSTAIN** | Sustain level | 0 - 100% | 70% |
| 9 | **WIDTH** | Stereo spread | 0 - 100% | 50% |
| 10 | **DETUNE** | Fine tuning | -100 to +100 cents | 0 cents |

---

## 🎵 Quick Start Presets

### Classic M1 Piano
```
DECAY: 40%, RELEASE: 30%, BRIGHT: 80%, RESO: 20%
VIBRATO: 0%, ATTACK: 1%, SUSTAIN: 70%, WIDTH: 50%
```
→ The classic M1 piano sound

### Bright Sparkly Piano
```
DECAY: 30%, RELEASE: 20%, BRIGHT: 95%, RESO: 30%
WIDTH: 70%
```
→ Bright, sparkly piano for pop/EDM

### Dark Moody Piano
```
DECAY: 60%, RELEASE: 50%, BRIGHT: 30%, SUSTAIN: 80%
```
→ Dark, melancholic, cinematic

### Percussive Stabs
```
DECAY: 10%, RELEASE: 5%, BRIGHT: 90%, RESO: 40%
SUSTAIN: 20%
```
→ For stabs and percussive hits

### Pad-like Piano
```
DECAY: 80%, RELEASE: 70%, BRIGHT: 60%, VIBRATO: 20%
ATTACK: 30%, SUSTAIN: 90%, WIDTH: 80%
```
→ Pad-like piano for ambient/atmosphere

---

## 📊 Technical Details

**Sample Data:**
- Source: m1-style-piano-shot_C_major.wav (96kHz original)
- Attack: 2160 samples (45ms @ 48kHz)
- Loop: 161 samples (single cycle @ 48kHz)
- Base pitch: 298.14 Hz (approximately C#4)
- Total memory: ~9KB for samples

**Synthesis:**
- Attack sample: One-shot playback on note-on
- Loop sample: Infinite loop after attack
- Pitch tracking: Automatic via MIDI note
- Interpolation: Linear sample interpolation

**Envelope:**
- 4-stage ADSR (Attack, Decay, Sustain, Release)
- Full parameter control

**Filter:**
- Type: 1-pole lowpass
- Cutoff: 100Hz - 20kHz (via Brightness)
- Resonance: 0 - 100% (feedback)

**LFO (Vibrato):**
- Waveform: Sine wave
- Depth: ±2% maximum pitch modulation
- Speed: 0.5 - 10 Hz

**Output:**
- Format: Mono (summed stereo internally)
- Stereo width: Adjustable 0-100%
- Soft clipping: For clean output

**Code Size:** 16KB (within 48KB limit)

---

## 📥 Installation

1. Download `m1_piano_osc.nts1mkiiunit`
2. Open KORG Librarian software
3. Upload to NTS-1 MKII oscillator slot
4. Select the oscillator on the NTS-1 MKII
5. Enjoy! 🎹

---

## 💡 Tips & Tricks

**Velocity Layering**
Use MIDI velocity for expression - the oscillator responds to velocity!

**Filter Sweeps**
Modulate the BRIGHTNESS parameter with the NTS-1's LFO for filter sweeps

**Percussive Hits**
Short DECAY + short RELEASE = percussive piano stabs

**Pad Sounds**
Long ATTACK + long DECAY/RELEASE + vibrato = pad-like sounds

**Resonance Sweet Spot**
30-40% resonance gives a nice "body" to the piano sound

**Stereo Width**
60-80% width gives a professional, wide piano sound

**Detune for Character**
+5 to +15 cents detune gives a characterful, slightly "out of tune" vibe

**Combine with NTS-1 Effects**
- Reverb: For natural space
- Delay: For rhythmic patterns
- Chorus: For extra width (careful with WIDTH parameter!)

---

## 🔧 Sample Extraction Process

For those interested in how the samples were extracted:

### 1. WAV Analysis
- 96kHz, mono, 16-bit original
- 1.1 seconds total length

### 2. Attack Extraction
- First 45ms of the sound
- Contains the hammer hit and initial transient
- Downsampled to 48kHz (2160 samples)

### 3. Loop Extraction
- Searched for stable part from 200ms onwards
- Single cycle extracted via zero-crossing detection
- Best match: 161 samples @ 48kHz
- Frequency: 298.14 Hz (approximately C#4)

### 4. C Code Generation
- Samples converted to float arrays
- Stored in `samples_data.h`
- Compiled directly into oscillator

**Tools Used:**
- `extract_samples_simple.py` (pure Python, no dependencies)
- Manual frequency verification

---

## 📁 Files

```
m1_piano_osc/
├── m1_piano_osc.nts1mkiiunit  (← Upload this!)
├── unit.cc                    (oscillator code)
├── header.c                   (parameter definitions)
├── samples_data.h             (sample data)
├── manifest.json
├── config.mk
├── Makefile
├── README.md
├── HOW_TO_m1_piano_osc.txt   (detailed manual)
└── extract_samples_simple.py  (sample extraction tool)
```

---

## 🚀 Building from Source

If you want to rebuild from source:

```bash
cd platform/nts-1_mkii/m1_piano_osc
./build_m1_piano_osc.sh
```

Requires Docker and WSL on Windows, or native Linux/Mac.

---

## 📝 Credits

- **Created by:** AI-assisted development
- **Build size:** 16KB
- **Sample data:** ~9KB
- **Code:** ~7KB
- **Platform:** KORG NTS-1 MKII

---

## 🎼 License

BSD 3-Clause License (following KORG logue-sdk license)

---

**Enjoy your M1-style piano!** 🎹✨
