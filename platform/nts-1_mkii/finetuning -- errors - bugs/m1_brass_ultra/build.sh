#!/bin/bash
# Build m1_brass_ultra unit

WORKSPACE_ROOT="/mnt/c/Users/sande_ej/logue-sdk"
UNIT="m1_brass_ultra"
UNIT_PATH="$WORKSPACE_ROOT/platform/nts-1_mkii/$UNIT"

echo "Building $UNIT..."

if [ ! -d "$UNIT_PATH" ]; then
    echo "  ✗ ERROR: Directory not found: $UNIT_PATH"
    exit 1
fi

# Build using Docker
docker run --rm \
    -v "$WORKSPACE_ROOT:/workspace" \
    korginc/logue-sdk:2.0.0 \
    /bin/bash -c "cd /workspace/platform/nts-1_mkii/$UNIT && make clean && make"

BUILD_EXIT=$?

# Check if file was created
if [ -f "$UNIT_PATH/${UNIT}.nts1mkiiunit" ]; then
    FILE_SIZE=$(stat -c%s "$UNIT_PATH/${UNIT}.nts1mkiiunit")
    echo "  ✓ SUCCESS - File created: ${UNIT}.nts1mkiiunit"
    echo "    Size: $FILE_SIZE bytes"
    ls -lh "$UNIT_PATH/${UNIT}.nts1mkiiunit"
    exit 0
else
    echo "  ✗ FAILED - File not created (exit code: $BUILD_EXIT)"
    exit 1
fi


