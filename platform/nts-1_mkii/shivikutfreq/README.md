# 🌀 SHIVIKUTFREQ - Frequency Shifting Dub Delay

**Ultimate Psychedelic Dub Delay with Real Frequency Shifter for Korg NTS-1 mkII**

---

## 🎛️ FEATURES

SHIVIKUTFREQ combines a high-quality delay with a **real frequency shifter** in the feedback path. Each repeat is shifted linearly in Hz (not semitones!), creating spiraling, gliding, otherworldly echoes that are perfect for:

- **Dub Techno/House** - Moving, evolving delays that stay in the groove
- **Psychedelic Production** - Spiraling echoes that detach from reality
- **Sound Design** - Frequency-shifting risers, drones, and textures
- **Experimental Music** - Inharmonic, sci-fi, metallic echo structures

### The Sound

- **Subtle shifts**: Gentle chorus-like detuning in the repeats
- **Medium shifts**: Clear upward/downward gliding echoes (sci-fi!)
- **Large shifts**: Extreme frequency spirals, metallic dub textures
- **High feedback**: Self-building frequency drones and risers

---

## 🎚️ PARAMETERS (10 Total)

### Hardware Knobs:
- **A Knob**: TIME - Delay time (10ms to 3s)
- **B Knob**: FEEDBACK - Feedback amount (subtle to infinite)
- **SHIFT+B**: MIX - Dry/wet balance (-100 to +100%)

### Additional Parameters (via menu):

**3. SHIFT** (0-100%)
- Frequency shift amount per repeat
- 0% = No shift (normal delay)
- 30% = Subtle movement (5-30 Hz)
- 100% = Extreme spirals (up to 100 Hz)

**4. DIRECT** (Direction)
- **OFF** - No frequency shift (normal delay mode)
- **UP** - Echo shifts upward in frequency (rising)
- **DOWN** - Echo shifts downward (falling)

**5. TONE** (0-100%)
- Filter color on repeats
- 0% = Dark, dubby (lowpass)
- 50% = Neutral
- 100% = Bright, metallic (highpass boost)

**6. STEREO** (0-100%)
- Stereo width of delays
- 0% = Mono
- 50% = Normal stereo
- 100% = Ultra-wide

**7. WANDER** (0-100%)
- Modulation/movement in delays
- Adds subtle to extreme psychedelic variation
- Low = Stable, tight
- High = Wandering, morphing echoes

**8. SYNC** (Tempo Sync)
- **OFF** - Free-running time
- **1/16** - Sixteenth notes
- **1/8** - Eighth notes  
- **3/16** - Dotted sixteenth
- **1/4** - Quarter notes
- **3/8** - Dotted eighth
- **1/2** - Half notes
- **3/4** - Dotted quarter
- **1/1** - Whole notes

**9. LOFI** (0-100%)
- Lo-fi/dirt on repeats
- Bit crushing and sample rate reduction
- Perfect for gritty dub textures

---

## 🎵 SOUND EXAMPLES & USAGE

### 1. Classic Dub Delay
```
TIME: 50% (1/4 note synced)
FEEDBACK: 60%
SHIFT: 15% (subtle)
DIRECT: UP
TONE: 30% (dark)
STEREO: 75%
SYNC: 1/4
```
Perfect for dub chords and stabs - subtle frequency rise adds movement.

### 2. Psychedelic Hi-Hat Delay
```
TIME: 30%
FEEDBACK: 70%
SHIFT: 40% (medium)
DIRECT: DOWN
TONE: 70% (bright)
WANDER: 50%
SYNC: 1/8
```
Hi-hats that spiral downward with wandering movement.

### 3. Frequency Riser/Drone
```
TIME: 70%
FEEDBACK: 90% (near-infinite!)
SHIFT: 25%
DIRECT: UP
TONE: 50%
STEREO: 100%
WANDER: 20%
```
Hold a chord, turn up feedback → self-building frequency drone!

### 4. Sci-Fi Synth Echo
```
TIME: 60%
FEEDBACK: 65%
SHIFT: 80% (extreme)
DIRECT: UP or DOWN
TONE: 80% (metallic)
LOFI: 30%
SYNC: 3/16
```
Synth lines that transform into otherworldly metallics.

### 5. Subtle Dub Movement
```
TIME: 55% (1/4 synced)
FEEDBACK: 50%
SHIFT: 10% (very subtle)
DIRECT: UP
TONE: 40%
STEREO: 60%
SYNC: 1/4
```
Barely noticeable shift - just adds "life" to standard delays.

---

## 🔬 TECHNICAL INFO

### Algorithm
- **Real Hilbert Transform** for 90° phase shifting
- **Single-Sideband Modulation** for frequency shifting
- Linear Hz shift (not pitch shift in semitones!)
- Per-repeat frequency shifting in feedback path

### Memory Usage
- 3 seconds maximum delay time
- ~2.3MB SDRAM allocated
- Optimized for NTS-1 mkII

### Processing
- 48 kHz sample rate
- Stereo input/output
- Low CPU usage (~15-20%)
- Stable feedback up to 93%

---

## 🛠️ INSTALLATION

### Requirements
- Korg NTS-1 mkII (firmware >= 1.0.0)
- KORG KONTROL Editor software
- USB-C cable

### Steps
1. Connect NTS-1 mkII to computer via USB
2. Open KORG KONTROL Editor
3. Navigate to Delay Effects section
4. Drag `shivikutfreq.nts1mkiiunit` to an empty user slot
5. Click "Send All" to transfer to device
6. Select SHIVIKUTFREQ on your NTS-1 mkII!

---

## 💡 TIPS & TRICKS

### For Dub Techno:
- Use SYNC mode for tight, groovy delays
- Keep SHIFT subtle (10-20%) for warm movement
- TONE at 30-40% for deep, dubby character
- Medium FEEDBACK (50-70%) for classic dub repeats

### For House:
- SYNC to 1/8 or 3/16 for house grooves
- SHIFT at 20-40% for clear frequency movement
- Higher TONE (60-80%) for bright, energetic delays
- STEREO at 75-100% for wide, spacious mix

### For Experimental/Ambient:
- High FEEDBACK (80-95%) for self-building textures
- Large SHIFT (60-100%) for extreme spirals
- WANDER at 50-80% for morphing soundscapes
- Try FREEZE mode by maxing feedback with subtle shift

### For Percussion:
- Short TIME (10-30%) with high SHIFT
- Creates metallic, robotic delay tails
- Perfect for hi-hats, claps, and FX hits
- Try DIRECT: DOWN for falling echoes

### Avoiding Runaway:
- Feedback is safely limited to 93% maximum
- Use TONE to tame harsh frequencies
- LOFI can add character while softening extremes
- If things get wild, reduce SHIFT amount first

---

## 🎛️ PARAMETER TIPS

**TIME**: 
- Short (0-20%) = Chorus/flanger territory
- Medium (20-60%) = Classic delay range
- Long (60-100%) = Dub/ambient delays

**FEEDBACK**:
- Low (0-40%) = 1-3 repeats, tight control
- Medium (40-70%) = Classic delay feedback
- High (70-90%) = Long tails, approaching infinite
- Extreme (90-93%) = Drone/riser territory

**SHIFT**:
- Tiny (0-10%) = Subtle detune/chorus effect
- Small (10-30%) = Noticeable but musical
- Medium (30-60%) = Clear frequency movement
- Large (60-100%) = Extreme sci-fi spirals

**DIRECT**:
- OFF = Normal delay (use for comparison)
- UP = Rising echoes (energetic, bright)
- DOWN = Falling echoes (dark, mysterious)

---

## 🔧 BUILDING FROM SOURCE

### Requirements
- logue SDK (v2.0+)
- GNU Arm Embedded Toolchain
- GNU Make

### Build
```bash
cd shivikutfreq
make clean
make
make install
```

Output: `shivikutfreq.nts1mkiiunit`

---

## 📚 RESOURCES

- [Korg logue SDK](https://github.com/korginc/logue-sdk)
- [NTS-1 mkII Product Page](https://www.korg.com/us/products/synthesizers/nts_1_mk2/)
- [Frequency Shifter Tutorial](https://www.perfectcircuit.com/signal/frequency-shifters)
- [Dub Delay Techniques](https://www.attackmagazine.com/technique/tutorials/creating-dub-delays-with-standard-plugins/)

---

## 🎸 GENRE APPLICATIONS

**Dub Techno**: Warm, moving delays that evolve with the groove  
**House**: Bright, rhythmic echoes with frequency movement  
**Ambient**: Self-building drones and evolving soundscapes  
**Experimental**: Inharmonic, sci-fi, otherworldly textures  
**Techno**: Metallic delays and risers for builds  
**Gabber**: Extreme frequency spirals on kicks and stabs

---

## ⚡ QUICK START

1. Start with **DIRECT: UP**, **SHIFT: 20%**, **FEEDBACK: 60%**
2. Set **SYNC** to match your tempo (try 1/4 or 1/8)
3. Adjust **TONE** for dark (dub) or bright (house)
4. Play with **SHIFT** amount to taste
5. Increase **FEEDBACK** slowly for longer tails
6. Add **WANDER** for movement and variation

**That's it! You're making spiraling dub delays!** 🌀🔥

---

## 📝 VERSION HISTORY

**v1.0.0** (2025-12-19)
- Initial release
- Real Hilbert transform frequency shifter
- 10 parameters including tempo sync
- Optimized for NTS-1 mkII
- Stable up to 93% feedback
- Lo-fi/dirt character option

---

## 🙏 CREDITS

Based on Korg logue SDK v2.0  
Frequency shifter algorithm inspired by classic analog designs  
Built with love for dub techno and psychedelic production

**Developed by**: Claude (Anthropic)  
**For**: Korg NTS-1 digital kit mkII  
**License**: BSD 3-Clause (see Korg logue SDK)

---

## 🌟 ENJOY SPIRALING INTO THE FREQUENCY COSMOS! 🌀✨

*"Where echoes don't just repeat... they evolve."*
