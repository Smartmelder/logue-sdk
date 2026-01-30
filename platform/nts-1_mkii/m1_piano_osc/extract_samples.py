#!/usr/bin/env python3
"""
Extract Attack and Loop samples from M1 Piano WAV
M1-Method: Separate attack (40-50ms) + single cycle loop
"""

import wave
import struct
import numpy as np

def find_zero_crossings(samples, start_idx):
    """Find zero crossings for clean loop points"""
    crossings = []
    for i in range(start_idx, len(samples) - 1):
        if samples[i] <= 0 and samples[i+1] > 0:
            crossings.append(i)
        if len(crossings) >= 10:
            break
    return crossings

def find_best_loop_cycle(samples, start_ms, sample_rate, base_freq=261.63):
    """
    Find the best single cycle for looping
    base_freq: C4 = 261.63 Hz (the note the sample was recorded at)
    """
    start_sample = int(start_ms * sample_rate / 1000)

    # Expected cycle length for C4 at this sample rate
    expected_cycle_len = int(sample_rate / base_freq)

    # Find zero crossings around the expected position
    search_start = start_sample
    crossings = find_zero_crossings(samples, search_start)

    if len(crossings) < 2:
        # Fallback: just take expected cycle length
        return samples[start_sample:start_sample + expected_cycle_len]

    # Find the crossing pair that's closest to expected length
    best_idx = 0
    best_diff = float('inf')

    for i in range(len(crossings) - 1):
        cycle_len = crossings[i+1] - crossings[i]
        diff = abs(cycle_len - expected_cycle_len)
        if diff < best_diff:
            best_diff = diff
            best_idx = i

    # Extract the cycle
    cycle_start = crossings[best_idx]
    cycle_end = crossings[best_idx + 1]

    return samples[cycle_start:cycle_end]

def extract_attack_and_loop(wav_path, attack_ms=45, loop_start_ms=200):
    """
    Extract attack and loop samples from WAV file

    Args:
        wav_path: Path to WAV file
        attack_ms: Length of attack in milliseconds (default 45ms)
        loop_start_ms: Where to start looking for loop cycle (default 200ms)
    """

    with wave.open(wav_path, 'rb') as wav:
        channels = wav.getnchannels()
        width = wav.getsampwidth()
        rate = wav.getframerate()
        frames = wav.getnframes()

        # Read all samples
        raw_data = wav.readframes(frames)

        # Convert to numpy array (assuming 16-bit)
        if width == 2:
            samples = np.frombuffer(raw_data, dtype=np.int16)
        else:
            raise ValueError(f"Unsupported sample width: {width}")

        # Convert to mono if stereo
        if channels == 2:
            samples = samples[::2]  # Take left channel

        # Normalize to float [-1.0, 1.0]
        samples = samples.astype(np.float32) / 32768.0

        print(f"\n=== WAV File Analysis ===")
        print(f"Sample Rate: {rate} Hz")
        print(f"Total Samples: {len(samples)}")
        print(f"Duration: {len(samples) / rate:.3f} seconds")
        print(f"Peak Level: {np.max(np.abs(samples)):.3f}")

        # Extract attack (first N milliseconds)
        attack_samples_count = int(attack_ms * rate / 1000)
        attack = samples[:attack_samples_count]

        print(f"\n=== Attack Extraction ===")
        print(f"Attack Length: {attack_ms}ms = {len(attack)} samples")
        print(f"Attack Peak: {np.max(np.abs(attack)):.3f}")

        # Find loop cycle
        loop_cycle = find_best_loop_cycle(samples, loop_start_ms, rate)

        print(f"\n=== Loop Cycle Extraction ===")
        print(f"Loop Start Search: {loop_start_ms}ms")
        print(f"Loop Cycle Length: {len(loop_cycle)} samples")
        print(f"Loop Frequency: {rate / len(loop_cycle):.2f} Hz (should be ~261.63 Hz for C4)")
        print(f"Loop Peak: {np.max(np.abs(loop_cycle)):.3f}")

        return attack, loop_cycle, rate

def downsample_to_48k(samples, original_rate, target_rate=48000):
    """Downsample from original rate to 48kHz (NTS-1 sample rate)"""
    if original_rate == target_rate:
        return samples

    # Simple linear interpolation for downsampling
    ratio = original_rate / target_rate
    new_len = int(len(samples) / ratio)

    indices = np.arange(new_len) * ratio
    downsampled = np.interp(indices, np.arange(len(samples)), samples)

    return downsampled

def generate_c_array(samples, array_name):
    """Generate C array code from samples"""
    # Format as C array
    lines = [f"const float {array_name}[] = {{"]

    # 8 values per line for readability
    for i in range(0, len(samples), 8):
        chunk = samples[i:i+8]
        values = ", ".join([f"{val:.6f}f" for val in chunk])
        lines.append(f"    {values},")

    lines.append("};")
    lines.append(f"const uint32_t {array_name}_len = {len(samples)};")

    return "\n".join(lines)

def main():
    wav_path = '/mnt/c/Users/sande_ej/logue-sdk/platform/nts-1_mkii/m1_piano_osc/m1-style-piano-shot_C_major.wav'

    # Extract samples
    attack, loop_cycle, rate = extract_attack_and_loop(wav_path)

    # Downsample to 48kHz (NTS-1 sample rate)
    print(f"\n=== Downsampling to 48kHz ===")
    attack_48k = downsample_to_48k(attack, rate, 48000)
    loop_48k = downsample_to_48k(loop_cycle, rate, 48000)

    print(f"Attack: {len(attack)} -> {len(attack_48k)} samples")
    print(f"Loop: {len(loop_cycle)} -> {len(loop_48k)} samples")

    # Generate C arrays
    print(f"\n=== Generating C Code ===")
    attack_code = generate_c_array(attack_48k, "k_attack_samples")
    loop_code = generate_c_array(loop_48k, "k_loop_samples")

    # Save to file
    output_path = '/mnt/c/Users/sande_ej/logue-sdk/platform/nts-1_mkii/m1_piano_osc/samples_data.h'

    with open(output_path, 'w') as f:
        f.write("/*\n")
        f.write(" * M1 Piano Samples - Auto-generated from WAV\n")
        f.write(" * Source: m1-style-piano-shot_C_major.wav\n")
        f.write(" * Base Note: C4 (261.63 Hz)\n")
        f.write(" */\n\n")
        f.write("#pragma once\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write("// Attack samples (first 45ms)\n")
        f.write(attack_code)
        f.write("\n\n")
        f.write("// Loop cycle samples (single cycle for looping)\n")
        f.write(loop_code)
        f.write("\n")

    print(f"\n✓ Generated: {output_path}")
    print(f"  - Attack: {len(attack_48k)} samples")
    print(f"  - Loop: {len(loop_48k)} samples")
    print(f"  - Total: {len(attack_48k) + len(loop_48k)} samples")
    print(f"  - Memory: ~{(len(attack_48k) + len(loop_48k)) * 4} bytes")

if __name__ == "__main__":
    main()
