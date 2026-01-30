#!/usr/bin/env python3
"""
Extract Attack and Loop samples from M1 Piano WAV
M1-Method: Separate attack (40-50ms) + single cycle loop
Pure Python - no dependencies
"""

import wave
import struct

def find_zero_crossings(samples, start_idx, count=10, min_distance=100):
    """Find zero crossings for clean loop points with minimum distance"""
    crossings = []
    last_crossing = -min_distance

    for i in range(start_idx, len(samples) - 1):
        if samples[i] <= 0 and samples[i+1] > 0:
            # Only add if it's far enough from the last crossing
            if i - last_crossing >= min_distance:
                crossings.append(i)
                last_crossing = i
        if len(crossings) >= count:
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

    print(f"  Expected cycle length: {expected_cycle_len} samples at {sample_rate}Hz")

    # Find zero crossings with minimum distance
    min_dist = int(expected_cycle_len * 0.8)  # At least 80% of expected length
    crossings = find_zero_crossings(samples, start_sample, 10, min_distance=min_dist)

    if len(crossings) < 2:
        # Fallback: just take expected cycle length
        print(f"  Warning: Not enough zero crossings, using expected length")
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

    print(f"  Found cycle from sample {cycle_start} to {cycle_end}")

    return samples[cycle_start:cycle_end]

def extract_attack_and_loop(wav_path, attack_ms=45, loop_start_ms=200):
    """
    Extract attack and loop samples from WAV file
    """

    with wave.open(wav_path, 'rb') as wav:
        channels = wav.getnchannels()
        width = wav.getsampwidth()
        rate = wav.getframerate()
        frames = wav.getnframes()

        # Read all samples
        raw_data = wav.readframes(frames)

        # Convert to list of floats (assuming 16-bit)
        if width == 2:
            # Unpack as signed 16-bit integers
            samples = list(struct.unpack(f'<{frames * channels}h', raw_data))
        else:
            raise ValueError(f"Unsupported sample width: {width}")

        # Convert to mono if stereo (take left channel)
        if channels == 2:
            samples = samples[::2]

        # Normalize to float [-1.0, 1.0]
        samples = [s / 32768.0 for s in samples]

        # Calculate peak
        peak = max(abs(s) for s in samples)

        print(f"\n=== WAV File Analysis ===")
        print(f"Sample Rate: {rate} Hz")
        print(f"Total Samples: {len(samples)}")
        print(f"Duration: {len(samples) / rate:.3f} seconds")
        print(f"Peak Level: {peak:.3f}")

        # Extract attack (first N milliseconds)
        attack_samples_count = int(attack_ms * rate / 1000)
        attack = samples[:attack_samples_count]

        attack_peak = max(abs(s) for s in attack)

        print(f"\n=== Attack Extraction ===")
        print(f"Attack Length: {attack_ms}ms = {len(attack)} samples")
        print(f"Attack Peak: {attack_peak:.3f}")

        # Find loop cycle
        loop_cycle = find_best_loop_cycle(samples, loop_start_ms, rate)

        loop_peak = max(abs(s) for s in loop_cycle)

        print(f"\n=== Loop Cycle Extraction ===")
        print(f"Loop Start Search: {loop_start_ms}ms")
        print(f"Loop Cycle Length: {len(loop_cycle)} samples")
        print(f"Loop Frequency: {rate / len(loop_cycle):.2f} Hz (should be ~261.63 Hz for C4)")
        print(f"Loop Peak: {loop_peak:.3f}")

        return attack, loop_cycle, rate

def downsample_to_48k(samples, original_rate, target_rate=48000):
    """Downsample from original rate to 48kHz (NTS-1 sample rate)"""
    if original_rate == target_rate:
        return samples

    # Linear interpolation for downsampling
    ratio = original_rate / target_rate
    new_len = int(len(samples) / ratio)

    downsampled = []
    for i in range(new_len):
        # Linear interpolation
        pos = i * ratio
        idx = int(pos)
        frac = pos - idx

        if idx + 1 < len(samples):
            val = samples[idx] * (1 - frac) + samples[idx + 1] * frac
        else:
            val = samples[idx]

        downsampled.append(val)

    return downsampled

def generate_c_array(samples, array_name):
    """Generate C array code from samples"""
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
