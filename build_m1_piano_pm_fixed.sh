#!/bin/bash
# Build m1_piano_pm_fixed unit (Workaround for permissions issue)

WORKSPACE_ROOT="/mnt/c/Users/sande_ej/logue-sdk"
UNIT="m1_piano_pm_fixed"
UNIT_PATH="$WORKSPACE_ROOT/platform/nts-1_mkii/$UNIT"

echo "Building $UNIT..."

if [ ! -d "$UNIT_PATH" ]; then
    echo "  ✗ ERROR: Directory not found: $UNIT_PATH"
    exit 1
fi

# PRE-CLEAN: Remove output file if it exists (fresh build)
rm -f "$UNIT_PATH/${UNIT}.nts1mkiiunit"

# Build using Docker with the correct entrypoint
docker run --rm \
    -v "$WORKSPACE_ROOT:/workspace" \
    -w /workspace \
    -h logue-sdk \
    xiashj/logue-sdk:latest \
    /app/cmd_entry build "platform/nts-1_mkii/$UNIT" > /tmp/build_${UNIT}.log 2>&1

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
    echo "  Last 30 lines of build log:"
    tail -30 /tmp/build_${UNIT}.log 2>/dev/null || echo "  (log not available)"
    exit 1
fi
