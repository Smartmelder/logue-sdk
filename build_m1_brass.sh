#!/bin/bash
# Build m1_brass and m1_brass_ultra units

WORKSPACE_ROOT="/mnt/c/Users/sande_ej/logue-sdk"

echo "Building m1_brass and m1_brass_ultra..."

# Build m1_brass
echo "Building m1_brass..."
UNIT_PATH="$WORKSPACE_ROOT/platform/nts-1_mkii/m1_brass"
docker run --rm \
    -v "$WORKSPACE_ROOT:/workspace" \
    -w /workspace \
    -h logue-sdk \
    xiashj/logue-sdk:latest \
    /app/cmd_entry build "platform/nts-1_mkii/m1_brass" > /tmp/build_m1_brass.log 2>&1

if [ -f "$UNIT_PATH/m1_brass.nts1mkiiunit" ]; then
    echo "  ✓ m1_brass.nts1mkiiunit created"
    ls -lh "$UNIT_PATH/m1_brass.nts1mkiiunit"
else
    echo "  ✗ m1_brass build failed"
    tail -20 /tmp/build_m1_brass.log 2>/dev/null
fi

# Build m1_brass_ultra
echo ""
echo "Building m1_brass_ultra..."
UNIT_PATH="$WORKSPACE_ROOT/platform/nts-1_mkii/m1_brass_ultra"
docker run --rm \
    -v "$WORKSPACE_ROOT:/workspace" \
    -w /workspace \
    -h logue-sdk \
    xiashj/logue-sdk:latest \
    /app/cmd_entry build "platform/nts-1_mkii/m1_brass_ultra" > /tmp/build_m1_brass_ultra.log 2>&1

if [ -f "$UNIT_PATH/m1_brass_ultra.nts1mkiiunit" ]; then
    echo "  ✓ m1_brass_ultra.nts1mkiiunit created"
    ls -lh "$UNIT_PATH/m1_brass_ultra.nts1mkiiunit"
else
    echo "  ✗ m1_brass_ultra build failed"
    tail -20 /tmp/build_m1_brass_ultra.log 2>/dev/null
fi

echo ""
echo "Build complete!"
