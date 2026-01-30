#!/bin/bash
# Build cathedral_smooth unit

WORKSPACE_ROOT="/mnt/c/Users/sande_ej/logue-sdk"
UNIT="cathedral_smooth"

echo "Building $UNIT..."

UNIT_PATH="$WORKSPACE_ROOT/platform/nts-1_mkii/$UNIT"

docker run --rm \
    -v "$WORKSPACE_ROOT:/workspace" \
    -w /workspace \
    -h logue-sdk \
    xiashj/logue-sdk:latest \
    /app/cmd_entry build "platform/nts-1_mkii/$UNIT" > /tmp/build_${UNIT}.log 2>&1

BUILD_EXIT=$?

if [ -f "$UNIT_PATH/${UNIT}.nts1mkiiunit" ]; then
    echo "  ✓ SUCCESS - File created: ${UNIT}.nts1mkiiunit"
    ls -lh "$UNIT_PATH/${UNIT}.nts1mkiiunit"
    SIZE=$(stat -f%z "$UNIT_PATH/${UNIT}.nts1mkiiunit" 2>/dev/null || stat -c%s "$UNIT_PATH/${UNIT}.nts1mkiiunit" 2>/dev/null || echo "unknown")
    echo "  File size: $SIZE bytes (~$((SIZE/1024))KB)"
    if [ "$SIZE" != "unknown" ] && [ "$SIZE" -gt 49152 ]; then
        echo "  ⚠ WARNING: File size exceeds 48KB limit!"
    fi
    exit 0
else
    echo "  ✗ FAILED - File not created (exit code: $BUILD_EXIT)"
    echo "  Last 30 lines of build log:"
    tail -30 /tmp/build_${UNIT}.log 2>/dev/null || echo "  (log not available)"
    exit 1
fi

