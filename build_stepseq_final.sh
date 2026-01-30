#!/bin/bash
# Build stepseq with proper environment setup

WORKSPACE_ROOT="/mnt/c/Users/sande_ej/logue-sdk"
UNIT="stepseq"

echo "Building $UNIT..."

# Remove old build artifacts
rm -f "$WORKSPACE_ROOT/platform/nts-1_mkii/$UNIT/${UNIT}.nts1mkiiunit"
rm -rf "$WORKSPACE_ROOT/platform/nts-1_mkii/$UNIT/build"

# Build using Docker with explicit EXTDIR
docker run --rm \
    -v "$WORKSPACE_ROOT:/workspace" \
    -w /workspace \
    -h logue-sdk \
    -e EXTDIR=/workspace/platform/ext \
    xiashj/logue-sdk:latest \
    bash -c "
        source /app/nts-1_mkii/environment nts-1_mkii 2>/dev/null || true
        cd /workspace/platform/nts-1_mkii/stepseq
        make clean
        make EXTDIR=/workspace/platform/ext
        make install EXTDIR=/workspace/platform/ext
    " 2>&1 | tee /tmp/build_stepseq_final.log

if [ -f "$WORKSPACE_ROOT/platform/nts-1_mkii/$UNIT/${UNIT}.nts1mkiiunit" ]; then
    echo ""
    echo "  ✓ SUCCESS - ${UNIT}.nts1mkiiunit created!"
    ls -lh "$WORKSPACE_ROOT/platform/nts-1_mkii/$UNIT/${UNIT}.nts1mkiiunit"
    exit 0
else
    echo ""
    echo "  ✗ FAILED - File not created"
    echo "  Last 30 lines of build log:"
    tail -30 /tmp/build_stepseq_final.log
    exit 1
fi

