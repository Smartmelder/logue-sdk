#!/bin/bash
# Rebuild all NTS-1 mkII units via WSL/Docker
# This script rebuilds all units to ensure .nts1mkiiunit files are up to date

set -e

UNITS=(
    "pan_trem"
    "freq_shift"
    "cathedral_smooth"
    "tr909"
    "td3_acid"
    "rave_engine"
    "orch_hit"
    "m1_piano_pm"
    "m1_brass_ultra"
    "m1_brass"
    "juno106"
    "jp8000"
    "gabber_bass"
    "disco_fall"
    "ultra_wide"
    "tape_wobble"
    "seq_filter"
    "rand_repeat"
    "kaoss_loop"
)

WORKSPACE_ROOT="/mnt/c/Users/sande_ej/logue-sdk"
FAILED=()
SUCCEEDED=()

echo "========================================"
echo "Rebuilding NTS-1 mkII units"
echo "Total units: ${#UNITS[@]}"
echo "========================================"
echo ""

for i in "${!UNITS[@]}"; do
    UNIT="${UNITS[$i]}"
    NUM=$((i + 1))
    echo "[$NUM/${#UNITS[@]}] Building $UNIT..."
    
    UNIT_PATH="$WORKSPACE_ROOT/platform/nts-1_mkii/$UNIT"
    
    if [ ! -d "$UNIT_PATH" ]; then
        echo "  ✗ ERROR: Directory not found"
        FAILED+=("$UNIT")
        continue
    fi
    
    # Build using Docker
    if docker run --rm \
        -v "$WORKSPACE_ROOT:/workspace" \
        -w /workspace \
        -h logue-sdk \
        xiashj/logue-sdk:latest \
        /app/cmd_entry build --nts-1_mkii "nts-1_mkii/$UNIT" > /tmp/build_${UNIT}.log 2>&1; then
        echo "  ✓ Success"
        SUCCEEDED+=("$UNIT")
    else
        echo "  ✗ Failed (check /tmp/build_${UNIT}.log)"
        FAILED+=("$UNIT")
    fi
done

echo ""
echo "========================================"
echo "Build Summary"
echo "========================================"
echo "Succeeded: ${#SUCCEEDED[@]}"
echo "Failed: ${#FAILED[@]}"

if [ ${#SUCCEEDED[@]} -gt 0 ]; then
    echo ""
    echo "Successfully rebuilt:"
    for unit in "${SUCCEEDED[@]}"; do
        echo "  ✓ $unit"
    done
fi

if [ ${#FAILED[@]} -gt 0 ]; then
    echo ""
    echo "Failed units:"
    for unit in "${FAILED[@]}"; do
        echo "  ✗ $unit"
    fi
fi

