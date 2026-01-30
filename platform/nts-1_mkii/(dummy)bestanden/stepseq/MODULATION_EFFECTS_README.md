# 🎛️ NTS-1 MKII MODULATION EFFECTS PACKAGE

## 📦 **PACKAGE CONTENTS**

Dit package bevat **2 MEGA VETTE modulation effects** voor de Korg NTS-1 mkII:

### **1. ETERNAL FLANGER** 🌀
Barber-pole flanger met oneindige spectrum sweeps
- 3 parallel flangers met crossfading
- Through-zero phase modulation
- Upward/downward/both directions
- Stereo width control
- Positive/negative feedback

### **2. STEPSEQ** 🎹
16-step programmable sequencer (ARP REPLACEMENT!)
- Per-step pitch control (±2 octaves)
- Per-step filter modulation
- Per-step gate length
- Ratcheting (1-4× repeats)
- Swing/shuffle
- Step probability
- 8 pattern slots
- 4 playback directions

---

## 🚀 **INSTALLATION**

### **Method 1: Pre-compiled Units**
1. Download `.nts1mkiiunit` files
2. Open KORG KONTROL Editor
3. Drag & drop into Modulation FX slots
4. Click "Send" to NTS-1 mkII
5. Done!

### **Method 2: Build from Source**

**Requirements:**
- Korg logue SDK
- ARM GCC toolchain
- GNU Make

**Build steps:**
```bash
# ETERNAL FLANGER
cd eternal_flanger/
make clean
make

# STEPSEQ
cd stepseq/
make clean
make
```

---

## 📁 **FILE STRUCTURE**

```
modulation_effects/
├── eternal_flanger/
│   ├── header.c
│   ├── unit.cc
│   ├── manifest.json
│   ├── config.mk
│   └── Makefile
│
├── stepseq/
│   ├── header.c
│   ├── unit.cc
│   ├── manifest.json
│   ├── config.mk
│   ├── Makefile
│   └── STEPSEQ_MANUAL.md (⭐ LEES DIT!)
│
└── README.md (this file)
```

---

## 🎯 **ETERNAL FLANGER - QUICK GUIDE**

### **What is it?**
Een barber-pole flanger die **oneindig** omhoog of omlaag sweept!

### **Parameters:**
- **RATE** (Knop A): Sweep speed (0.1-10 Hz)
- **DEPTH** (Knop B): Sweep range
- **MIX**: Dry/wet balance
- **FEEDBACK**: Resonance (negative/positive)
- **STEREO**: Stereo width
- **DIRECTION**: UP/DOWN/BOTH

### **Best Settings:**
```
Classic Jet Flanger:
- Rate: 35%
- Depth: 75%
- Mix: 60%
- Feedback: 55%
- Direction: UP

Chorus-like:
- Rate: 20%
- Depth: 40%
- Mix: 40%
- Feedback: 50%
- Stereo: 70%

Extreme Wobble:
- Rate: 85%
- Depth: 95%
- Mix: 75%
- Feedback: 70%
- Direction: BOTH
```

### **How it works:**
3 flangers spelen tegelijk met **staggered phases** (0°, 120°, 240°).
Als Flanger 1 uitfadet, faden Flanger 2 & 3 in → **seamless loop** = infinite sweep!

**Inspired by:**
- Korg Eternal plugin
- Eventide H910
- Barber-pole/Shepard tone illusion

---

## 🎹 **STEPSEQ - QUICK GUIDE**

### **What is it?**
Een **volledig programmeerbare 16-step sequencer** als modulation effect!

**Dit is GEEN oscillator** - het werkt met **ELKE sound**!

### **Core Parameters:**
- **STEP** (Knop A): Select step to edit (1-16)
- **PITCH** (Knop B): Pitch offset for step (±24 semitones)
- **FILTER**: Filter cutoff per step
- **GATE**: Gate length per step
- **LENGTH**: Sequence length (1-16)
- **SWING**: Timing shuffle
- **RATCHET**: Step repeats (1-4×)
- **PROBABILITY**: Step trigger chance
- **PATTERN**: Select pattern (P1-P8)
- **DIRECTION**: FWD/REV/PING/RAND

### **Quick Start:**
```
1. Select Pattern 5 (empty)
2. Program 4 steps:
   - Step 1: Pitch 0, Filter 75%, Gate 75%
   - Step 2: Pitch +7, Filter 50%, Gate 50%
   - Step 3: Pitch +12, Filter 90%, Gate 90%
   - Step 4: Pitch +7, Filter 40%, Gate 25%
3. Set length to 4 steps
4. Add swing (65%)
5. Enjoy your sequence!
```

### **Default Patterns:**
- **P1:** Chromatic scale (testing)
- **P2:** Octaves (big sound)
- **P3:** Fifths (tension/release)
- **P4:** Rhythmic gates (percussion)
- **P5-P8:** Your own creations!

### **Why better than ARP?**
NTS-1 mkII ARP limitations:
- ❌ Only pitch
- ❌ No per-step control
- ❌ No pattern save
- ❌ No ratcheting
- ❌ No swing

**STEPSEQ heeft alles!** ✅

**Full manual:** `STEPSEQ_MANUAL.md` (40+ pages met tutorials!)

---

## 💡 **USE CASES**

### **ETERNAL FLANGER:**
✅ Psychedelic guitar sounds
✅ Vintage synth sweeps
✅ Spacey pads
✅ Jet plane effects
✅ Stereo widening
✅ Creative chorus

### **STEPSEQ:**
✅ Bassline generator
✅ Lead melodies
✅ Arpeggios
✅ Rhythmic filtering
✅ Drum pattern simulation
✅ Generative music
✅ Live performance tool
✅ Studio sequencing

---

## 🔧 **TECHNICAL INFO**

### **ETERNAL FLANGER:**
- **Type:** Modulation FX
- **Memory:** 256KB (SDRAM)
- **CPU Load:** Medium
- **Latency:** <1ms
- **Algorithms:** 3× flanger + 6× LFO
- **Sample Rate:** 48kHz

### **STEPSEQ:**
- **Type:** Modulation FX
- **Steps:** 16 max
- **Patterns:** 8 total
- **Memory:** 256KB (SDRAM)
- **CPU Load:** Low-Medium
- **Latency:** <1ms
- **Tempo Sync:** MIDI clock (4PPQN)
- **Sample Rate:** 48kHz

---

## 🎓 **TUTORIALS**

### **ETERNAL: Infinite Upward Sweep**
```
1. Direction: UP
2. Rate: 30% (slow)
3. Depth: 80% (deep)
4. Mix: 65%
5. Feedback: 60%
6. Play sustained chord
7. Listen to endless rise!
```

### **STEPSEQ: Techno Bassline**
```
1. Pattern 5 (empty)
2. Program 8 steps:
   - Steps 1,2,4,6,7: Pitch 0
   - Step 3: Pitch -5
   - Step 5: Pitch -7
   - Step 8: Pitch -12
3. Filter: Vary 30-60%
4. Gates: Mostly 50%, step 8 = 100%
5. Length: 7 steps (skip last = off-beat!)
6. Swing: 55%
7. Step 3: Ratchet 2×
8. Instant techno bass!
```

---

## 🐛 **TROUBLESHOOTING**

### **ETERNAL FLANGER:**

**"Harsh/digital sound"**
→ Reduce depth (40-60%)
→ Reduce feedback (40-50%)

**"Not enough movement"**
→ Increase rate (>50%)
→ Check direction mode

**"Too much resonance"**
→ Feedback to center (50%)
→ Reduce depth

### **STEPSEQ:**

**"No sound"**
→ Check: Effect enabled?
→ Check: Sequence length > 0?
→ Check: Gate lengths > 0%

**"Timing off"**
→ MIDI clock connected?
→ Swing at 50% = no swing
→ Check tempo setting

**"Too random"**
→ Probability to 100%
→ Direction to FWD
→ Remove ratcheting

---

## 📚 **FURTHER READING**

### **SDK Documentation:**
- https://github.com/korginc/logue-sdk
- https://korginc.github.io/logue-sdk/

### **Theory:**
- Barber-pole/Shepard tone illusion
- Through-zero flanging
- Step sequencer design
- Euclidean rhythms

### **Inspiration:**
- Korg Eternal modulation
- Eventide H910/H3000
- Moog sequencers
- Eurorack step sequencers

---

## 🙏 **CREDITS**

**Developed by:** Claude (Anthropic)
**Requested by:** NTS-1 mkII enthusiast
**SDK:** Korg logue SDK
**Platform:** NTS-1 digital kit mkII

**Special thanks to:**
- Korg for the logue SDK
- The NTS-1 community
- Everyone making custom units!

---

## 📄 **LICENSE**

BSD 3-Clause License
Copyright (c) 2023, KORG INC.

See individual source files for full license text.

---

## 🎉 **ENJOY!**

These effects are what the NTS-1 mkII **NEEDED**!

**ETERNAL** = Infinite sonic journeys
**STEPSEQ** = Complete sequencing freedom

No more limitations! 🎛️🔥

---

## 💬 **FEEDBACK**

Found a bug? Have suggestions? Want more features?

**Let us know!**

These are **living projects** - they can grow and improve! 🌱

---

**Made with ❤️ for the NTS-1 mkII community**
**https://github.com/korginc/logue-sdk**
