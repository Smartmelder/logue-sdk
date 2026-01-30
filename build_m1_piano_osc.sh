#!/bin/bash
# Build m1_piano_osc unit - M1-style Sample-Based Piano

WORKSPACE_ROOT="/mnt/c/Users/sande_ej/logue-sdk"
UNIT="m1_piano_osc"

echo "════════════════════════════════════════════════════════════════"
echo "Building M1 PIANO - Sample-Based Oscillator (M1 Method)"
echo "════════════════════════════════════════════════════════════════"
echo ""

UNIT_PATH="$WORKSPACE_ROOT/platform/nts-1_mkii/$UNIT"

docker run --rm \
    -v "$WORKSPACE_ROOT:/workspace" \
    -w /workspace \
    -h logue-sdk \
    xiashj/logue-sdk:latest \
    /app/cmd_entry build "platform/nts-1_mkii/$UNIT" > /tmp/build_${UNIT}.log 2>&1

BUILD_EXIT=$?

if [ $BUILD_EXIT -eq 0 ]; then
    echo "✓ Build successful!"
    echo ""
    echo "Output file:"
    ls -lh "$UNIT_PATH/${UNIT}.nts1mkiiunit" 2>/dev/null

    if [ -f "$UNIT_PATH/${UNIT}.nts1mkiiunit" ]; then
        echo ""
        echo "Unit size: $(stat -f%z "$UNIT_PATH/${UNIT}.nts1mkiiunit" 2>/dev/null || stat -c%s "$UNIT_PATH/${UNIT}.nts1mkiiunit" 2>/dev/null) bytes"

        # Copy to github_upload folder
        mkdir -p "$WORKSPACE_ROOT/github_upload/oscillators"
        cp "$UNIT_PATH/${UNIT}.nts1mkiiunit" "$WORKSPACE_ROOT/github_upload/oscillators/" 2>/dev/null

        echo ""
        echo "════════════════════════════════════════════════════════════════"
        echo "✓ M1 PIANO built successfully!"
        echo "✓ Also copied to github_upload/oscillators/ folder"
        echo "════════════════════════════════════════════════════════════════"
    fi
else
    echo "✗ Build failed!"
    echo ""
    echo "Build log:"
    cat /tmp/build_${UNIT}.log
    echo ""
    echo "════════════════════════════════════════════════════════════════"
    echo "✗ Build failed - check errors above"
    echo "════════════════════════════════════════════════════════════════"
    exit 1
fi
