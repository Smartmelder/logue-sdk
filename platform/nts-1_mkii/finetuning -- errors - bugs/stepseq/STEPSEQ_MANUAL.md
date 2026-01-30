# 🎹 STEPSEQ - ULTIMATE STEP SEQUENCER MODULATION

## 🔥 **WAT IS DIT?**

**STEPSEQ** is een **volledig programmeerbare 16-step sequencer** als modulation effect!

Dit is **GEEN oscillator** - het werkt met **ELKE oscillator/sound** die je hebt!

### **Wat doet het?**
- ✅ **Per-step pitch control** (-24 tot +24 semitones)
- ✅ **Per-step filter modulation** (ritmisch filteren)
- ✅ **Per-step gate length** (envelope per step)
- ✅ **Ratcheting** (herhaal steps 1-4×)
- ✅ **Swing/shuffle** (timing offset)
- ✅ **Step probability** (controlled randomness)
- ✅ **8 patterns** met instant recall
- ✅ **Tempo sync** via MIDI clock
- ✅ **4 playback modes**: Forward/Reverse/Ping-Pong/Random

### **Waarom is dit beter dan de ARP?**
De ARP van de NTS-1 mkII is beperkt tot:
- Alleen pitch
- Geen per-step control
- Geen pattern save
- Geen ratcheting
- Geen probability

**STEPSEQ geeft je VOLLEDIGE CONTROLE!** 🎛️

---

## 🎛️ **PARAMETERS**

### **KNOP A (Parameter 0): STEP SELECT**
- **Functie:** Selecteer welke step je wilt programmeren (1-16)
- **Range:** 0-15 (wordt weergegeven als 01-16)
- **Gebruik:** Draai knop A om door de steps te bladeren

### **KNOP B (Parameter 1): PITCH OFFSET**
- **Functie:** Stel pitch offset in voor geselecteerde step
- **Range:** -24 tot +24 semitones (2 octaven omhoog/omlaag)
- **Gebruik:** Creëer melodische sequenties!
- **Voorbeeld:**
  - Step 1: 0 semitones (root note)
  - Step 2: +7 semitones (perfect fifth)
  - Step 3: +12 semitones (octave)
  - Step 4: +7 semitones
  - → Instant arpeggio!

### **Parameter 2: FILTER MOD**
- **Functie:** Stel filter cutoff modulation in voor geselecteerde step
- **Range:** 0-100% (0% = closed, 100% = open)
- **Gebruik:** Creëer ritmische filter patterns!
- **⚠️ FIXED:** Filter cutoff is nu veilig gelimiteerd (100Hz-12kHz) om geluidsuitval te voorkomen
- **Voorbeeld:**
  - Step 1: 0% (filter closed - geen geluid)
  - Step 2: 50% (half open)
  - Step 3: 100% (volledig open)
  - → Ritmisch filter effect!
- **Functie:** Filter cutoff modulation voor geselecteerde step
- **Range:** 0-100%
- **Effect:** Moduleert het low-pass filter
- **Gebruik:** Creëer ritmische filter sweeps
- **Voorbeeld:**
  - Hoge steps (75-100%): Bright, open sound
  - Lage steps (0-25%): Muffled, bass-heavy

### **Parameter 3: GATE LENGTH**
- **Functie:** Hoe lang de step klinkt
- **Range:** 0-100%
- **Effect:** Korte gates = staccato, lange gates = legato
- **Gebruik:**
  - 25%: Short, punchy notes
  - 75%: Smooth, connected notes
  - 100%: Full sustain

### **Parameter 4: SEQUENCE LENGTH**
- **Functie:** Hoe veel steps worden afgespeeld
- **Range:** 1-16 steps
- **Gebruik:**
  - 4 steps: Verse pattern
  - 8 steps: Chorus pattern
  - 16 steps: Complex fills
- **TIP:** Wissel tussen lengtes voor song structure!

### **Parameter 5: SWING**
- **Functie:** Timing offset voor oneven steps
- **Range:** 0-100% (50% = geen swing)
- **Effect:** Creëert "groovy" feeling
- **Gebruik:**
  - 50%: Straight timing (geen swing)
  - 60-70%: Light shuffle
  - 75%+: Heavy swing (triplet feel)

### **Parameter 6: RATCHET**
- **Functie:** Herhaal geselecteerde step meerdere keren
- **Options:** 1×, 2×, 3×, 4×
- **Effect:** Verdubbelt/verdriedubbelt de step
- **Gebruik:** Geweldig voor drum rolls, rapid arpeggios
- **Voorbeeld:**
  - Step 1: 1× (normaal)
  - Step 4: 4× (rapid fire!)
  - → Instant fill!

### **Parameter 7: PROBABILITY**
- **Functie:** Kans dat step wordt afgespeeld
- **Range:** 0-100%
- **Effect:** Controlled randomness
- **Gebruik:**
  - 100%: Altijd afspelen
  - 50%: 50/50 kans
  - 25%: Zeldzaam
- **TIP:** Gebruik voor "almost euclidean" rhythms!

### **Parameter 8: PATTERN SELECT**
- **Functie:** Kies tussen 8 vooraf geprogrammeerde patterns
- **Options:** P1-P8
- **Gebruik:** Instant recall tijdens live performance!
- **Default patterns:**
  - P1: Chromatic scale (muzikaal)
  - P2: Octaves (big sound)
  - P3: Fifths (tension/release)
  - P4: Rhythmic gates (percussion-achtig)
  - P5-P8: Jouw eigen creaties!

### **Parameter 9: DIRECTION**
- **Functie:** Playback direction van sequencer
- **Options:**
  - **FWD:** Forward (1→16)
  - **REV:** Reverse (16→1)
  - **PING:** Ping-pong (1→16→1)
  - **RAND:** Random steps
- **Gebruik:** Instant variations zonder pattern aanpassen!

### **Parameter 10: PLAY** ⭐ **NIEUW!**
- **Functie:** Start/stop de sequencer
- **Options:**
  - **PLAY:** Sequencer draait (modulation actief)
  - **PAUSE:** Sequencer gestopt (pass-through mode)
- **Gebruik:**
  - **PLAY:** Start sequencer voor live modulation
  - **PAUSE:** Stop sequencer, audio gaat direct door (geen modulation)
- **⚠️ BELANGRIJK:** Zonder PLAY = geen sequencer! Zet deze op PLAY om te starten!
- **TIP:** Gebruik PAUSE voor breaks in je performance, dan weer PLAY voor instant restart!

---

## 🎯 **QUICK START GUIDE**

### **⚠️ BELANGRIJK: PLAY PARAMETER!**
**VOOR JE BEGINT:** Zet Parameter 10 (PLAY) op **PLAY**! Zonder dit werkt de sequencer NIET!

1. **Select STEPSEQ** als modulation effect
2. **Zet Parameter 10 (PLAY) op PLAY** ⭐ **KRITIEK!**
3. **Program je pattern:**
   - Select step (Knop A)
   - Stel pitch in (Knop B)
   - Stel filter in (Parameter 2)
   - Herhaal voor alle steps
4. **Stel LENGTH in** (Parameter 4): 4-16 steps
5. **Geniet van je sequencer!** 🎹

### **STAP 1: Basis Sequentie Maken**

1. **Selecteer Pattern 5** (lege pattern):
   - Parameter 8 → draai naar P5

2. **Programmeer 4 steps:**
   - **Step 1:**
     - Knop A → 00 (step 1)
     - Knop B → 0 (root note)
     - Param 2 → 75% (bright filter)
     - Param 3 → 75% (medium gate)
   
   - **Step 2:**
     - Knop A → 01 (step 2)
     - Knop B → +7 (fifth up)
     - Param 2 → 50% (mid filter)
     - Param 3 → 50% (shorter gate)
   
   - **Step 3:**
     - Knop A → 02 (step 3)
     - Knop B → +12 (octave up)
     - Param 2 → 90% (very bright)
     - Param 3 → 90% (long gate)
   
   - **Step 4:**
     - Knop A → 03 (step 4)
     - Knop B → +7 (fifth up)
     - Param 2 → 40% (darker)
     - Param 3 → 25% (staccato!)

3. **Set sequence length:**
   - Parameter 4 → 3 (gebruik 4 steps)

4. **Press play en hoor je sequence!** 🎵

---

### **STAP 2: Add Swing**

- Parameter 5 → 65%
- Nu hebben oneven steps timing offset
- Instant groove!

---

### **STAP 3: Add Ratcheting**

- Knop A → 03 (step 4)
- Parameter 6 → 4× (quadruple)
- Step 4 speelt nu 4× zo snel!
- Perfect voor fills!

---

### **STAP 4: Add Randomness**

- Knop A → 01 (step 2)
- Parameter 7 → 50%
- Step 2 speelt nu maar 50% van de tijd
- Controlled chaos!

---

## 🎸 **ADVANCED TECHNIQUES**

### **TECHNIQUE 1: Euclidean Rhythms**

Gebruik **probability** om euclidean-achtige rhythms te maken:

```
Step 1: 100% prob
Step 2: 33% prob
Step 3: 100% prob
Step 4: 33% prob
Step 5: 100% prob
Step 6: 33% prob
...
```

Resultaat: Onregelmatige maar muzikale patronen!

---

### **TECHNIQUE 2: Call & Response**

Gebruik **direction** voor instant variations:

1. Programmeer 8-step pattern
2. FWD = "Call" (vraag)
3. REV = "Response" (antwoord)
4. Wissel tussen FWD/REV voor song structure!

---

### **TECHNIQUE 3: Polyrhythms**

Maak 2 patterns met verschillende lengtes:

- Pattern 1: 5 steps (5/16)
- Pattern 2: 7 steps (7/16)
- Wissel tussen patterns voor polyrhythmische feel!

---

### **TECHNIQUE 4: Ratchet Fills**

Laatste step van pattern = ratcheting:

```
Steps 1-15: Normaal (1×)
Step 16: Ratchet 4×
```

Resultaat: Automatic fill elke pattern repeat!

---

### **TECHNIQUE 5: Filter Sequences**

Alleen filter modulation gebruiken:

```
Alle steps: Pitch offset = 0
Filter mod: Variërend per step
```

Resultaat: Ritmisch filter zonder pitch changes!
Geweldig voor techno/house!

---

## 🔧 **TROUBLESHOOTING**

### **"Ik hoor niks!"**
- Check: Is Modulation Effect enabled?
- Check: Mix level (parameter 2 in MOD menu)
- Check: Sequence length (param 4) > 0?

### **"Timing is niet strak!"**
- Zorg dat MIDI clock actief is
- Check: Tempo sync in NTS-1 settings
- Swing op 50% = no swing

### **"Steps klinken allemaal hetzelfde!"**
- Check: Heb je per step verschillende waardes geprogrammeerd?
- Check: Filter mod range (probeer extremen: 0% en 100%)

### **"Te chaotisch!"**
- Probability terug naar 100% (param 7)
- Direction naar FWD (param 9)
- Swing naar 50% (param 5)

---

## 💡 **CREATIVE IDEAS**

### **IDEA 1: Bassline Generator**
```
Pitch offsets: 0, -5, -7, -12 (bass notes)
Filter: Laag (20-40%)
Gates: Kort (25-50%)
Ratchet op step 4
```

### **IDEA 2: Lead Melody**
```
Pitch offsets: 0, +3, +7, +10, +12 (muzikaal)
Filter: Hoog (70-90%)
Gates: Lang (75-100%)
Swing: 60%
```

### **IDEA 3: Rhythmic Texture**
```
Alle pitch offsets: 0
Filter: Random (20-90%)
Gates: Random (10-90%)
Probability: 70% (sommige steps skippen)
Direction: RAND
```

### **IDEA 4: Drum Pattern Simulation**
```
Step 1,5,9,13: Pitch +12, Filter 90% (hi-hat)
Step 3,7,11,15: Pitch -12, Filter 30% (kick)
Step 5,13: Pitch 0, Filter 60% (snare)
Gates: Alle 25% (staccato)
```

---

## 🎵 **PATTERN LIBRARY**

### **Pattern 1 (Default): Chromatic Scale**
Steps gaan chromatisch omhoog
Perfect voor testen!

### **Pattern 2 (Default): Octaves**
Springt tussen octaves
Big, powerful sound!

### **Pattern 3 (Default): Fifths**
Muzikale intervallen
Tension & release!

### **Pattern 4 (Default): Rhythmic Gates**
Alleen gate modulation
Pure rhythm!

### **Patterns 5-8: YOUR CREATIONS!**
Sla hier je eigen patterns op!

---

## 🚀 **PERFORMANCE TIPS**

### **Live Performance**
1. Programmeer 8 patterns voor volledige song
2. Pattern 1-2: Intro/verse (simpel)
3. Pattern 3-4: Build-up (complexer)
4. Pattern 5-6: Drop (maximaal)
5. Pattern 7-8: Breakdown/outro

### **Jamming**
- Gebruik RAND direction voor instant variation
- Adjust swing real-time voor groove changes
- Toggle ratcheting on/off voor dynamics

### **Recording**
- Start met simpele pattern (4 steps)
- Bouw complexity op door:
  1. Lengere sequence
  2. Add swing
  3. Add probability
  4. Add ratcheting
- Record hele progression!

---

## 📊 **TECHNICAL SPECS**

- **Steps:** 16 maximum
- **Patterns:** 8 total
- **Pitch range:** ±2 octaves
- **Filter range:** 20Hz - 20kHz
- **Tempo range:** 60-240 BPM (auto-sync via MIDI)
- **Sample rate:** 48kHz
- **Latency:** <1ms

---

## ⚡ **KEYBOARD SHORTCUTS**

(Via MIDI CC mapping - setup in NTS-1)

- **CC 1:** Quick step select
- **CC 2:** Quick pitch offset
- **CC 3:** Pattern select
- **CC 4:** Direction toggle

---

## 🎓 **WORKFLOW EXAMPLE**

**Making a techno bassline:**

1. **Select Pattern 5**
2. **Program 8 steps:**
   ```
   Step 1: Pitch 0, Filter 40%, Gate 75%
   Step 2: Pitch 0, Filter 30%, Gate 25%
   Step 3: Pitch -5, Filter 50%, Gate 90%
   Step 4: Pitch 0, Filter 25%, Gate 25%
   Step 5: Pitch -7, Filter 60%, Gate 75%
   Step 6: Pitch 0, Filter 35%, Gate 50%
   Step 7: Pitch 0, Filter 45%, Gate 25%
   Step 8: Pitch -12, Filter 70%, Gate 100%
   ```
3. **Set length: 7** (skip last step = off-beat!)
4. **Swing: 55%**
5. **Step 3: Ratchet 2×** (emphasis)
6. **Step 6: Probability 75%** (occasional skip)

**Result:** Groovy, evolving techno bassline! 🔊

---

## 🌟 **FINAL THOUGHTS**

**STEPSEQ** is niet zomaar een step sequencer - het is een **compositie tool**!

### **Gebruik het voor:**
- ✅ Basslines
- ✅ Lead melodies  
- ✅ Rhythmic textures
- ✅ Drum patterns (via filter/gate)
- ✅ Experimental sound design
- ✅ Live performance
- ✅ Studio production

### **Het werkt met:**
- ✅ Alle NTS-1 oscillators
- ✅ Externe audio (via input)
- ✅ Samples
- ✅ Andere synths (via routing)

---

## 🎉 **ENJOY YOUR NEW SEQUENCER!**

Dit is wat de NTS-1 mkII **ECHT NODIG HAD**!

Geen beperkingen meer - **volledige controle**! 🎛️🔥

---

**Made with ❤️ for the NTS-1 mkII community**
**Based on Korg logue SDK**
**https://github.com/korginc/logue-sdk**
