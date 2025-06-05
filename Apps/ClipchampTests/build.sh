#!/bin/bash

# Build script for Clipchamp BabylonNative Tests
# This script builds the test suite and runs basic validation

set -e

echo "Building Clipchamp BabylonNative Tests..."
echo "========================================"

# Navigate to the BabylonNative directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BABYLON_NATIVE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "BabylonNative directory: $BABYLON_NATIVE_DIR"

# Build BabylonNative first (if not already built)
if [ ! -d "$BABYLON_NATIVE_DIR/Build" ]; then
    echo "Building BabylonNative dependencies..."
    cd "$BABYLON_NATIVE_DIR"
    
    # Create build directory
    mkdir -p Build
    cd Build
    
    # Configure with CMake
    cmake .. -DCMAKE_BUILD_TYPE=Debug
    
    # Build
    make -j$(nproc) || make -j$(sysctl -n hw.ncpu) || make -j4
    
    echo "BabylonNative dependencies built successfully."
else
    echo "BabylonNative dependencies already built."
fi

# Build ClipchampTests
echo "Building ClipchampTests..."
cd "$SCRIPT_DIR"

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build
make -j$(nproc) || make -j$(sysctl -n hw.ncpu) || make -j4

echo "ClipchampTests built successfully!"

# Run tests if requested
if [ "$1" = "--run" ]; then
    echo "Running tests..."
    echo "================"
    ./ClipchampBabylonNativeTests
fi

echo "Build completed successfully!"
echo "To run tests: cd build && ./ClipchampBabylonNativeTests"
