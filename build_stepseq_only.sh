#!/bin/bash
# Build only stepseq unit

WORKSPACE_ROOT="/mnt/c/Users/sande_ej/logue-sdk"
UNIT="stepseq"

echo "Building $UNIT..."

UNIT_PATH="$WORKSPACE_ROOT/platform/nts-1_mkii/$UNIT"

if [ ! -d "$UNIT_PATH" ]; then
    echo "  ✗ ERROR: Directory not found: $UNIT_PATH"
    exit 1
fi

# Build using Docker (continue even if env backup has permission issues)
docker run --rm \
    -v "$WORKSPACE_ROOT:/workspace" \
    -w /workspace \
    -h logue-sdk \
    xiashj/logue-sdk:latest \
    /app/cmd_entry build "platform/nts-1_mkii/$UNIT" > /tmp/build_stepseq.log 2>&1

BUILD_RESULT=$?

# Check if file was created despite potential warnings
if [ -f "$UNIT_PATH/stepseq.nts1mkiiunit" ]; then
    echo "  ✓ Success - stepseq.nts1mkiiunit created"
    exit 0
elif [ $BUILD_RESULT -eq 0 ]; then
    echo "  ✓ Build completed (exit code 0)"
    exit 0
else
    echo "  ✗ Failed (exit code $BUILD_RESULT)"
    echo "  Last 20 lines of build log:"
    tail -20 /tmp/build_stepseq.log 2>/dev/null || echo "  (log not available)"
    exit 1
fi
    echo "  ✓ Success"
    exit 0
else
    echo "  ✗ Failed"
    exit 1
fi

