# ADVANCED STEP SEQUENCER - NTS-1 mkII

## Build

Plaats directory in: `logue-sdk/platform/nts-1_mkii/advseq/`

```bash
cd logue-sdk/platform/nts-1_mkii/advseq
make clean
make
```

## Upload

Upload `advseq.nts1mkiiunit` via KORG KONTROL Editor

## Usage

1. Select effect in MOD FX menu
2. Set SEQLEN (param 1) voor aantal steps (1-128)
3. Gebruik OPER (param 5) voor pattern generation:
   - NONE: Geen operatie
   - RAND: Randomize sequence
   - SHUF: Shuffle sequence
   - REV: Reverse sequence
   - COPY: Copy slice over sequence
   - CSHUF: Copy shuffled slice
   - PCOPY: Palindrome copy (SLICECILS)
   - PSHUF: Palindrome shuffle
4. SHIFT (param 6) om pattern te verschuiven (-64 tot +64)
5. Adjust MIX voor dry/wet balance
6. Set CLOCK (param 0) voor 1/16 of MIDI trigger mode
7. Adjust SWING (param 8) voor timing variation

## Features

- 128-step programmable sequence
- 8 pattern operations
- Shift left/right with wrap-around
- Palindrome (SLICECILS) patterns
- Tempo sync (1/16 or MIDI trigger)
- Swing/shuffle timing
- Smooth glide between steps
- Variable sequence length (1-128 steps)
- Slice operations (1-32 steps)

