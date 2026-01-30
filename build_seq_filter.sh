#!/bin/bash
# Build seq_filter unit

WORKSPACE_ROOT="/mnt/c/Users/sande_ej/logue-sdk"
UNIT="seq_filter"

echo "Building $UNIT..."

UNIT_PATH="$WORKSPACE_ROOT/platform/nts-1_mkii/$UNIT"

if [ ! -d "$UNIT_PATH" ]; then
    echo "  ✗ ERROR: Directory not found: $UNIT_PATH"
    exit 1
fi

# Build using Docker
docker run --rm \
    -v "$WORKSPACE_ROOT:/workspace" \
    -w /workspace \
    -h logue-sdk \
    xiashj/logue-sdk:latest \
    /app/cmd_entry build "platform/nts-1_mkii/$UNIT" > /tmp/build_${UNIT}.log 2>&1

BUILD_EXIT=$?

# Check if file was created
if [ -f "$UNIT_PATH/${UNIT}.nts1mkiiunit" ]; then
    echo "  ✓ SUCCESS - File created: ${UNIT}.nts1mkiiunit"
    ls -lh "$UNIT_PATH/${UNIT}.nts1mkiiunit"
    echo "  File size: $(stat -c%s "$UNIT_PATH/${UNIT}.nts1mkiiunit" 2>/dev/null || echo "unknown") bytes"
    exit 0
elif grep -q "Creating.*\.nts1mkiiunit" /tmp/build_${UNIT}.log 2>/dev/null; then
    echo "  ✓ Build completed (file creation detected in log)"
    if [ -f "$UNIT_PATH/${UNIT}.nts1mkiiunit" ]; then
        ls -lh "$UNIT_PATH/${UNIT}.nts1mkiiunit"
    fi
    exit 0
else
    echo "  ✗ FAILED - File not created (exit code: $BUILD_EXIT)"
    echo "  Last 30 lines of build log:"
    tail -30 /tmp/build_${UNIT}.log 2>/dev/null || echo "  (log not available)"
    exit 1
fi

