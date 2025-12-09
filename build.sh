#!/bin/bash

echo "========================================="
echo "  N64 System Information Tool - Builder"
echo "========================================="
echo ""

# Check if N64_INST is set
if [ -z "$N64_INST" ]; then
    echo "❌ ERROR: N64_INST environment variable not set"
    echo ""
    echo "Please install libdragon and source the environment:"
    echo "  source /path/to/libdragon/env.sh"
    echo ""
    exit 1
fi

echo "✓ N64 toolchain found at: $N64_INST"
echo ""

# Clean previous build
echo "Cleaning previous build..."
make clean > /dev/null 2>&1

# Build the ROM
echo "Building N64 ROM..."
echo ""

if make; then
    echo ""
    echo "========================================="
    echo "✓ Build successful!"
    echo "========================================="
    echo ""
    echo "ROM file created: n64-sysinfo.z64"
    echo ""
    echo "Next steps:"
    echo "  1. Test in emulator (Project64, Mupen64Plus, Ares)"
    echo "  2. Copy to flash cart SD card for real hardware"
    echo ""
    
    # Show file size
    if [ -f "n64-sysinfo.z64" ]; then
        SIZE=$(ls -lh n64-sysinfo.z64 | awk '{print $5}')
        echo "ROM Size: $SIZE"
    fi
else
    echo ""
    echo "========================================="
    echo "❌ Build failed!"
    echo "========================================="
    echo ""
    echo "Please check the error messages above."
    exit 1
fi
