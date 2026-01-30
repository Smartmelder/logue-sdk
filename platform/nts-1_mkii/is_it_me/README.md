# IS IT ME - Melancholic Reverb

A professional, melancholic reverb effect for the **KORG NTS-1 MKII**.

![Status](https://img.shields.io/badge/Status-Stable-green)
![Platform](https://img.shields.io/badge/Platform-NTS--1_MKII-blue)
![Type](https://img.shields.io/badge/Type-Reverb-orange)
![Size](https://img.shields.io/badge/Size-9KB-brightgreen)

---

## Features

✅ **Pristine Sound Quality** - No distortion or unwanted coloration
✅ **Bass Exclusion** - Highpass filter keeps the low-end tight
✅ **3 Reverb Modes** - ROOM, HALL, CATHEDRAL with distinct characters
✅ **10 Parameters** - Complete control over the reverb characteristics
✅ **ARP/SEQ Compatible** - Works perfectly with sequencers and arpeggiators
✅ **Stable** - No clicks, pops, or glitches

---

## Quick Start

### Build Instructions

```bash
# Using Docker (recommended)
cd /path/to/logue-sdk
./build_is_it_me_docker.sh

# Manual build
cd platform/nts-1_mkii/is_it_me
make clean
make install
```

### Installation

1. Upload `is_it_me.nts1mkiiunit` to your NTS-1 MKII via **KORG Librarian**
2. Select the effect on your NTS-1 MKII reverb slot
3. Tweak parameters to taste!

---

## Parameters

| Control | Parameter | Description | Range |
|---------|-----------|-------------|-------|
| **Knob A** | TIME | Reverb decay time | Short → Long (0.5-8 sec) |
| **Knob B** | DEPTH | Reverb amount/intensity | Subtle → Lush |
| **Param 0** | MIX | Dry/Wet balance | -100% → +100% |
| **Param 1** | SIZE | Room size | Small → Large |
| **Param 2** | DAMP | HF damping | Bright → Dark |
| **Param 3** | DIFFUSE | Diffusion amount | Discrete → Dense |
| **Param 4** | PREDLY | Pre-delay time | 0 → 250ms |
| **Param 5** | EARLY | Early reflections | Subtle → Realistic |
| **Param 6** | HP | Highpass (bass exclusion) | 30Hz → 500Hz |
| **Param 7** | LP | Lowpass filter | 1kHz → 12kHz |
| **Param 8** | MODE | Reverb character | ROOM / HALL / CATHEDRAL |

---

## Reverb Modes

### 🏠 ROOM
- Small, intimate space
- Fast decay
- Perfect for: drums, percussive sounds, tight mixes

### 🎭 HALL
- Medium-sized venue (DEFAULT)
- Balanced decay
- Perfect for: synths, pads, vocals, general use

### ⛪ CATHEDRAL
- Large, ethereal space
- Slow, lush decay
- Perfect for: ambient, atmospheric productions, drones

---

## Preset Suggestions

### Tight Techno Room
```
TIME: 40%, DEPTH: 30%, MIX: -60%
SIZE: 30%, DAMP: 60%, DIFFUSE: 40%
HP: 25%, MODE: ROOM
```
Short, tight reverb for drums and bass.

### Lush Ambient Hall
```
TIME: 85%, DEPTH: 70%, MIX: -20%
SIZE: 75%, DAMP: 40%, DIFFUSE: 80%
HP: 15%, MODE: HALL
```
Large, full reverb for pads and synths.

### Melancholic Cathedral
```
TIME: 95%, DEPTH: 60%, MIX: 0%
SIZE: 90%, DAMP: 70%, DIFFUSE: 65%
EARLY: 40%, HP: 20%, MODE: CATHEDRAL
```
Dark, emotional reverb for introspective music.

### Bass-Exclusive Reverb
```
TIME: 60%, DEPTH: 50%, MIX: -30%
HP: 35%, LP: 80%, MODE: HALL
```
Only mids/highs have reverb - bass stays dry and tight!

---

## Technical Details

**Algorithm:**
- Hybrid Schroeder + Dattorro reverb topology
- 6 parallel comb filters (stereo pairs with prime number delays)
- 4 series allpass diffusers
- Early reflection network (6 taps)
- Pre-delay buffer (max 250ms)
- Biquad HP/LP filters (applied to reverb send only)
- Soft clipping for stability

**Specifications:**
- Sample Rate: 48 kHz
- Memory Usage: ~180 KB SDRAM
- Code Size: ~9 KB (well within 48 KB limit)
- Latency: Minimal (pre-delay dependent)

**Compatibility:**
- ✅ KORG NTS-1 MKII
- ✅ ARP mode
- ✅ SEQ mode
- ✅ MIDI input
- ✅ All oscillators

---

## Why "IS IT ME"?

The name reflects the **introspective, melancholic** nature of this reverb effect. It's designed to add emotional depth to your music without distorting the original sound. The question "Is it me?" mirrors the self-reflective atmosphere this reverb creates.

This reverb is perfect for:
- Emotional, introspective music
- Ambient and atmospheric productions
- Clean, professional reverb tails
- Keeping bass tight while adding space to highs

---

## Build Information

**Built for:** KORG NTS-1 MKII
**Type:** Reverb Effect (revfx)
**Version:** 1.0.0
**Compiler:** GCC ARM 10.3

---

## Documentation

See **`HOW_TO_is_it_me.txt`** for detailed parameter descriptions, tips, and extensive usage examples.

---

## License

BSD 3-Clause License
Copyright (c) 2024

---

## Support

For questions, issues, or feature requests, please refer to the main logue-sdk repository.

**Enjoy creating melancholic soundscapes! 🎵**
