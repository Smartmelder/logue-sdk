# 🌧️ WATERKUT - Raindrop Delay Effect

**A chaotic raindrop delay for the Korg NTS-1 mkII**

## 🎵 Concept

WATERKUT is a unique delay effect that simulates the naturally chaotic sound of raindrops falling onto a surface. It features 10 delay lines connected in series with proportional feedback, where each delay time is randomized to create organic, evolving textures.

### Features

- **10 Series Delay Lines**: Connected like water drops cascading through multiple surfaces
- **Randomized Timing**: Each delay has a random offset for natural chaos
- **Proportional Feedback**: Feedback decreases through the chain, mimicking natural decay
- **Built-in Modulation**: Adds dimension and unison detune
- **Alternating Polarity**: Modulation alternates per delay line for width
- **Wide Range**: From early reflections to dense reverb with long tails

## 🎛️ Parameters

### TIME (Knob A)
- **Range:** 0.1 - 3.0 seconds
- **Effect:** Controls the overall delay time scale
- **Use:**
  - **Low (0-30%):** Quick reflections, slapback delays
  - **Mid (30-70%):** Classic delay times, rhythmic patterns
  - **High (70-100%):** Long delays, reverb-like tails

### DEPTH (Knob B)  
- **Range:** 0 - 95%
- **Effect:** Controls the feedback amount
- **Use:**
  - **Low (0-30%):** Single echoes, short decay
  - **Mid (30-60%):** Multiple repeats, musical feedback
  - **High (60-95%):** Infinite repeats, reverb density

### MOD INTENSITY (Shift + Time)
- **Range:** 0 - 50%
- **Effect:** Controls modulation depth and chorus width
- **Use:**
  - **Low (0-20%):** Subtle movement, slight detune
  - **Mid (20-40%):** Chorus effect, dimension
  - **High (40-50%):** Strong modulation, warble

### CHAOS (Shift + Depth)
- **Range:** 0 - 100%
- **Effect:** Controls randomization amount
- **Use:**
  - **Low (0-30%):** Regular, predictable delays
  - **Mid (30-70%):** Natural variation, "raindrop" character
  - **High (70-100%):** Maximum chaos, random textures

## 🎯 Suggested Settings

### **Light Rain** (Early reflections)
```
TIME:     20%
DEPTH:    30%
MOD INT:  15%
CHAOS:    40%
```
Creates subtle early reflections like gentle rain on a window.

### **Heavy Downpour** (Dense delay)
```
TIME:     60%
DEPTH:    70%
MOD INT:  30%
CHAOS:    60%
```
Thick, chaotic delay texture simulating heavy rain.

### **Water Cave** (Reverb-like)
```
TIME:     85%
DEPTH:    85%
MOD INT:  25%
CHAOS:    50%
```
Long, reverberant tails with natural variation.

### **Crystal Drops** (Shimmer)
```
TIME:     40%
DEPTH:    60%
MOD INT:  45%
CHAOS:    30%
```
High modulation creates shimmering, crystalline delays.

### **Rhythmic Rain** (Sync pattern)
```
TIME:     30%
DEPTH:    50%
MOD INT:  10%
CHAOS:    20%
```
Lower chaos for more predictable, rhythmic delays.

## 🔧 Technical Details

### Architecture
- **10 delay lines** in series configuration
- **Exponential spacing**: Each line has progressively longer base time
- **Proportional feedback**: Decreases from 80% to 8% through the chain
- **Sample-accurate interpolation**: Linear interpolation for smooth delay reads
- **Soft clipping**: Prevents feedback explosion
- **Stereo spread**: Modulation creates natural width

### Memory Usage
- **Buffer size:** ~6MB SDRAM
- **Max delay time:** 3 seconds total
- **Sample rate:** 48kHz

### DSP Features
- XORShift random generator for chaos
- Sine wave modulation oscillator
- Alternating polarity per delay line
- Tanh soft clipping for saturation
- Linear interpolation for delay reads

## 📝 Building

Place all files in the NTS-1 mkII SDK directory structure:

```
logue-sdk/platform/nts-1_mkii/waterkut/
├── config.mk
├── header.c
├── unit.cc
├── waterkut.h
├── waterkut.cpp
├── manifest.json
└── README.md
```

Then compile:

```bash
cd logue-sdk/platform/nts-1_mkii/waterkut
make
```

This will generate `waterkut.nts1mkiiunit` ready to load onto your NTS-1 mkII.

## 🎹 Usage Tips

1. **Start Simple**: Begin with low CHAOS (20-30%) to understand the base delay character
2. **Add Chaos Gradually**: Increase CHAOS to add natural variation
3. **Modulation Sweet Spot**: 25-35% MOD INTENSITY works well for most sounds
4. **Feedback Control**: Keep DEPTH below 80% to avoid infinite feedback
5. **Stereo Width**: Higher MOD INTENSITY creates wider stereo image
6. **Tempo Sync**: The effect respects MIDI clock for synchronized timing

## 🌟 Sound Design Ideas

### Ambient Textures
High TIME + High DEPTH + High CHAOS = Evolving, organic soundscapes

### Rhythmic Delays
Low TIME + Medium DEPTH + Low CHAOS = Tight, musical delays

### Reverb Simulation  
High TIME + High DEPTH + Medium CHAOS = Reverb-like space

### Shimmer Effect
Medium TIME + High DEPTH + High MOD INTENSITY = Crystalline shimmer

### Natural Spaces
Medium TIME + Medium DEPTH + Medium CHAOS = Realistic room ambience

## ⚠️ Notes

- **Feedback Warning**: At high DEPTH settings (>85%), delays can build up significantly
- **CPU Usage**: 10 delay lines use significant processing - normal for this effect
- **Chaos Regeneration**: Changing CHAOS parameter regenerates random offsets
- **Modulation Phase**: Stereo width is tied to modulation oscillator phase

## 🔗 Resources

- [Korg logue SDK](https://github.com/korginc/logue-sdk)
- [NTS-1 mkII Product Page](https://www.korg.com/products/synthesizers/nts_1_mk2/)

---

**Created for the NTS-1 mkII community** 🌧️✨

Enjoy the sound of digital rain!

