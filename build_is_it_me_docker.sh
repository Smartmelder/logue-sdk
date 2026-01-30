#!/bin/bash
# Build is_it_me unit - Melancholic Reverb for NTS-1 MKII

WORKSPACE_ROOT="/mnt/c/Users/sande_ej/logue-sdk"
UNIT="is_it_me"

echo "════════════════════════════════════════════════════════════════"
echo "Building IS IT ME - Melancholic Reverb for NTS-1 MKII"
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

if [ -f "$UNIT_PATH/${UNIT}.nts1mkiiunit" ]; then
    echo "✓ BUILD SUCCESSFUL!"
    echo ""
    echo "Output file: platform/nts-1_mkii/$UNIT/${UNIT}.nts1mkiiunit"
    ls -lh "$UNIT_PATH/${UNIT}.nts1mkiiunit"
    echo ""
    SIZE=$(stat -f%z "$UNIT_PATH/${UNIT}.nts1mkiiunit" 2>/dev/null || stat -c%s "$UNIT_PATH/${UNIT}.nts1mkiiunit" 2>/dev/null || echo "unknown")
    echo "File size: $SIZE bytes (~$((SIZE/1024))KB)"

    if [ "$SIZE" != "unknown" ] && [ "$SIZE" -gt 49152 ]; then
        echo ""
        echo "⚠ WARNING: File size exceeds 48KB limit!"
        echo "   The NTS-1 MKII may reject this unit."
        echo "   Consider optimizing the code to reduce size."
    else
        echo ""
        echo "✓ File size is within the 48KB limit"
    fi

    echo ""
    echo "════════════════════════════════════════════════════════════════"
    echo "Next steps:"
    echo "1. Upload '${UNIT}.nts1mkiiunit' to your NTS-1 MKII via KORG Librarian"
    echo "2. Select the effect on your NTS-1 MKII"
    echo "3. Read HOW_TO_is_it_me.txt for parameter descriptions and tips"
    echo "════════════════════════════════════════════════════════════════"
    echo ""

    # Copy to github_upload folder if it exists
    if [ -d "$WORKSPACE_ROOT/github_upload/revfx" ]; then
        cp "$UNIT_PATH/${UNIT}.nts1mkiiunit" "$WORKSPACE_ROOT/github_upload/revfx/"
        echo "✓ Also copied to github_upload/revfx/ folder"
        echo ""
    fi

    exit 0
else
    echo "✗ BUILD FAILED!"
    echo ""
    echo "Build log:"
    cat /tmp/build_${UNIT}.log
    echo ""
    echo "════════════════════════════════════════════════════════════════"
    exit 1
fi
