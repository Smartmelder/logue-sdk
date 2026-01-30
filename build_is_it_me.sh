#!/bin/bash

# Build script for IS IT ME reverb effect
# NTS-1 MKII - Melancholic Reverb

echo "════════════════════════════════════════════════════════════════"
echo "Building IS IT ME - Melancholic Reverb for NTS-1 MKII"
echo "════════════════════════════════════════════════════════════════"

cd platform/nts-1_mkii/is_it_me || exit 1

echo ""
echo "Cleaning previous build..."
make clean

echo ""
echo "Building reverb effect..."
make

if [ $? -eq 0 ]; then
    echo ""
    echo "Installing..."
    make install

    if [ -f "is_it_me.nts1mkiiunit" ]; then
        echo ""
        echo "════════════════════════════════════════════════════════════════"
        echo "✓ BUILD SUCCESSFUL!"
        echo "════════════════════════════════════════════════════════════════"
        echo ""
        echo "Output file: platform/nts-1_mkii/is_it_me/is_it_me.nts1mkiiunit"
        echo ""
        echo "File size:"
        ls -lh is_it_me.nts1mkiiunit
        echo ""
        echo "Next steps:"
        echo "1. Upload 'is_it_me.nts1mkiiunit' to your NTS-1 MKII via KORG Librarian"
        echo "2. Select the effect on your NTS-1 MKII"
        echo "3. Read HOW_TO_is_it_me.txt for parameter descriptions and tips"
        echo ""
        echo "════════════════════════════════════════════════════════════════"
    else
        echo ""
        echo "✗ ERROR: Output file not created!"
        exit 1
    fi
else
    echo ""
    echo "✗ BUILD FAILED!"
    echo "Check the error messages above for details."
    exit 1
fi
